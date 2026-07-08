# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Helper to create GPT partition tables, VMDK descriptors, and partition images for QEMU.

Design Rationale:
-----------------
Android/Cuttlefish partition images (.img files) are normally aggregated into a single
composite disk format by Cuttlefish's 'cvd' host tools for use with 'crosvm'. Since we
are running Fuchsia inside QEMU (which then runs Android inside a Starnix container),
we cannot use the crosvm-specific composite disk format.

To solve this, this module:
1. Dynamically constructs a standard GUID Partition Table (GPT) in memory based on the
   actual sizes of the partition images extracted from the Cuttlefish ZIP.
2. Generates a VMware VMDK descriptor file ('disk.vmdk') in 'monolithicFlat' format.
   This descriptor stitches the separate partition image files into a single virtual disk
   at QEMU runtime, preventing the need to physically copy or merge these large files.

Uses Python's struct module to pack binary structures.
See https://docs.python.org/3/library/struct.html#format-characters for format details.
"""

import binascii
import logging
import os
import struct
import uuid


def make_gpt_header(disk_sectors, part_entry_crc, disk_guid):
  header_size = 92

  # Temporary header with 0 CRC as a mutable bytearray
  header_data = bytearray(struct.pack(
      '<8sIIIIQQQQ16sQIII',  # GPT Header format
      b'EFI PART',           # Signature
      0x00010000,            # Revision (1.0)
      header_size,           # Header Size
      0,                     # Header CRC32 (temp 0)
      0,                     # Reserved
      1,                     # Current LBA
      disk_sectors - 1,      # Backup LBA
      34,                    # First Usable LBA
      disk_sectors - 34,     # Last Usable LBA
      disk_guid,             # Disk GUID
      2,                     # Partition Entries LBA
      128,                   # Number of Partition Entries
      128,                   # Size of Partition Entry
      part_entry_crc         # Partition Entries CRC32
  )).ljust(512, b'\x00')     # Pad to 512 bytes

  # Calculate CRC and overwrite the temporary 0 CRC at offset 16
  struct.pack_into('<I', header_data, 16, binascii.crc32(header_data[:header_size]))

  return header_data


def make_partition_entry(name, start_lba, end_lba, part_guid, type_guid):
  name_bytes = name.encode('utf-16le').ljust(72, b'\x00')[:72]

  return struct.pack(
      '<16s16sQQQ72s',  # GPT Partition Entry format
      type_guid,        # Partition Type GUID
      part_guid,        # Unique Partition GUID
      start_lba,        # Starting LBA
      end_lba,          # Ending LBA
      0,                # Attributes
      name_bytes        # Partition Name
  )


def create_gpt_and_vmdk(partitions_list, output_gpt_path, output_vmdk_path):
  """Creates a GPT header binary file and a VMDK flat descriptor.

  Args:
    partitions_list: List of dicts:
      { 'name': 'boot_a', 'path': 'relative/path/to/boot.img', 'size_bytes': N }
  """
  disk_guid = uuid.uuid4().bytes_le

  # LBA 0: MBR (empty protective MBR)
  mbr = bytearray(512)
  mbr[0x1fe:0x200] = b'\x55\xaa'

  # Calculate sectors for each partition
  sector_size = 512
  current_lba = 34 # GPT headers + entries take 34 sectors (LBA 0 to 33)

  gpt_entries = []
  vmdk_extents = []

  # Add LBA 0-33 as the first extent in VMDK (gpt_header.raw)
  vmdk_extents.append(f'RW 34 FLAT "{os.path.basename(output_gpt_path)}" 0')

  for p in partitions_list:
    if not os.path.exists(p['path']):
      raise FileNotFoundError(f"Partition file does not exist: {p['path']}")
    sectors = (os.path.getsize(p['path']) + sector_size - 1) // sector_size
    # Align partition to 8 sectors (4KB)
    aligned_sectors = (sectors + 7) & ~7

    start_lba = current_lba
    end_lba = start_lba + aligned_sectors - 1

    part_guid = uuid.uuid4().bytes_le
    # ebd0a0a2-b9e5-4433-87c0-68b6b72699c7 is the standard GPT Partition Type
    # GUID for a Basic Data Partition.
    type_guid = p.get('type_guid', uuid.UUID('ebd0a0a2-b9e5-4433-87c0-68b6b72699c7').bytes_le)
    entry = make_partition_entry(p['name'], start_lba, end_lba, part_guid, type_guid)
    gpt_entries.append(entry)

    # VMDK extent mapping
    # Note: If the partition size is not perfectly aligned to sector size,
    # we still map the whole aligned sectors. VMDK handles flat file mapping.
    vmdk_extents.append(f'RW {aligned_sectors} FLAT "{p["path"]}" 0')

    # Update current LBA
    current_lba = end_lba + 1

  # Create backup_gpt.raw file of zeros
  backup_gpt_path = os.path.join(os.path.dirname(output_gpt_path), 'backup_gpt.raw')
  with open(backup_gpt_path, 'wb') as f:
    f.write(b'\x00' * (34 * 512))
  vmdk_extents.append(f'RW 34 FLAT "{os.path.basename(backup_gpt_path)}" 0')

  # Pad partition entries to 128 entries
  while len(gpt_entries) < 128:
    gpt_entries.append(bytearray(128))

  # Concatenate partition entries
  entries_data = b''.join(gpt_entries)
  part_entry_crc = binascii.crc32(entries_data)

  # Make GPT header
  disk_sectors = current_lba + 34 # Usable sectors + backup GPT at the end

  # Set first partition record in protective MBR (type 0xee).
  # Limit sector count to 32-bit max.
  mbr[0x1be:0x1be+16] = struct.pack(
      '<B3BB3BII',  # Protective MBR Partition Record format
      0x00, 0x00, 0x02, 0x00, 0xee, 0xff, 0xff, 0xff, 1,
      min(0xffffffff, disk_sectors - 1))

  # Write protective MBR + primary GPT header + partition entries to output file
  with open(output_gpt_path, 'wb') as f:
    f.write(mbr)
    f.write(make_gpt_header(disk_sectors, part_entry_crc, disk_guid))
    f.write(entries_data)

  # Write VMDK file (monolithicFlat format)
  with open(output_vmdk_path, 'w') as f:
    f.write(f"""# Disk DescriptorFile
version=1
CID={binascii.hexlify(uuid.uuid4().bytes[:4]).decode()}
parentCID=ffffffff
createType="monolithicFlat"

# Extent description
""" + "\n".join(vmdk_extents) + f"""
# The Backup GPT is appended by mapping a flat file of zeros or just let it be
# (QEMU doesn't strictly check backup GPT unless primary is corrupted).
""")

  logging.info(f"GPT header created at {output_gpt_path}")
  logging.info(f"VMDK descriptor created at {output_vmdk_path}")


def create_uboot_env_image(env_dict, output_path, env_size=4096, partition_size=4096):
  """Compiles U-Boot environment variables and writes them to a partition image."""
  env_data = bytearray()
  for k, v in env_dict.items():
    env_data.extend(f"{k}={v}".encode('utf-8'))
    env_data.append(0)
  env_data.append(0)

  if len(env_data) > env_size - 4:
    raise ValueError("Environment size exceeds limit")

  env_data.extend([0xff] * (env_size - 4 - len(env_data)))
  env_blob = struct.pack('<I', binascii.crc32(env_data)) + env_data

  with open(output_path, 'wb') as f:
    f.write(env_blob + b'\x00' * (partition_size - len(env_blob)))
  logging.info(f"U-Boot environment image created at {output_path}")


def create_zero_image(output_path, size_mb):
  """Creates an empty zero-filled image file of the specified size in MB."""
  with open(output_path, 'wb') as f:
    f.truncate(size_mb * 1024 * 1024)
  logging.info(f"Created zero-filled sparse image at {output_path} ({size_mb}MB)")


def create_misc_image(output_path):
  """Creates a 16MB misc.img initialized with BootloaderControl metadata."""
  # Slot info for slot_a, slot_b, slot_c, slot_d:
  # slot_a: priority 15, tries 0, success 1 (b'\x8f\x00')
  # slot_b: priority 14, tries 7, success 0 (b'\x7e\x00')
  # slot_c/slot_d: inactive (b'\x00\x00')
  header = (
      struct.pack(
          '<4sIBBB',
          b'_a\x00\x00',  # slot_suffix
          0x42414342,     # magic (BOOT_CTRL_MAGIC)
          1,              # version
          2,              # byte9: nb_slot=2, recovery_tries=0
          0               # byte10: merge_status=0
      )
      + b'\x00'           # reserved0
      + b'\x8f\x00'       # slot_a
      + b'\x7e\x00'       # slot_b
      + b'\x00\x00'       # slot_c
      + b'\x00\x00'       # slot_d
      + b'\x00' * 8       # reserved1
  )
  boot_control_data = header + struct.pack('<I', binascii.crc32(header))

  with open(output_path, 'wb') as f:
    f.write(b'\x00' * 2048)
    f.write(boot_control_data)
    f.write(b'\x00' * ((16 * 1024 * 1024) - 2048 - len(boot_control_data)))
  logging.info(f"Initialized misc.img with BootloaderControl metadata at {output_path}")
