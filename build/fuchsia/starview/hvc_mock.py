# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Mock host-side proxy responder for Cuttlefish HVC console serial ports.

Design Rationale:
-----------------
In a standard Cuttlefish deployment, guest services (like operator, tombstones, keymaster)
communicate with host-side helper daemons over multiple Hypervisor Console (HVC) serial ports
(e.g., /dev/hvc1, /dev/hvc2) exposed via VirtIO Serial.
Certain guest boot daemons write initialization packets to these ports and block indefinitely
until they receive a handshake response from the host.

Since we boot Cuttlefish directly in QEMU without the host CVD daemon running, there would normally
be nothing listening on these ports, causing Android boot to hang.

This module resolves this by:
1. Creating named pipes (FIFOs) on the host for each HVC port.
2. Binding these pipes as backends for QEMU's virtconsole devices.
3. Spawning a background thread that handles the handshake protocol (reading commands from the guest
   and replying with the expected response packet headers containing a success code '\x00'),
   allowing guest boot services to proceed without blocking.
"""

import logging
import os
import struct
import threading
import time


NUM_PORTS = 11


def start_hvc_mock_responder(temp_dir):
  """Spawns a mock host-side responder for Cuttlefish HVC ports.

  Creates named FIFOs in temp_dir, maps them to QEMU virtconsoles,
  and starts a daemon thread that handles guest handshakes.

  Args:
    temp_dir: Temporary directory where FIFOs will be created.

  Returns:
    A tuple of (qemu_args, stop_event), where:
      - qemu_args: List of command line arguments for QEMU.
      - stop_event: A threading.Event to signal the responder thread to terminate.
  """
  stop_event = threading.Event()
  fds = []
  qemu_args = []

  # Add VirtIO Serial PCI controller
  qemu_args.extend([
      '-device', 'virtio-serial-pci-non-transitional,id=virtio-serial0,max_ports=31',
  ])

  for i in range(NUM_PORTS):
    base_path = os.path.join(temp_dir, f'hvc{i}_fifo')
    fifo_in = base_path + '.in'
    fifo_out = base_path + '.out'

    if not os.path.exists(fifo_in):
      os.mkfifo(fifo_in)
    if not os.path.exists(fifo_out):
      os.mkfifo(fifo_out)

    qemu_args.extend([
        '-chardev', f'pipe,id=hvc{i},path={base_path}',
        '-device', f'virtconsole,bus=virtio-serial0.0,chardev=hvc{i},id=hvc{i}',
    ])

    # hvc0 is the kernel console (printk log outputs) and hvc1 is the
    # interactive serial console. Since these are raw text consoles rather
    # than binary CVD IPC endpoints, we must not run the mock handshake responder
    # on them.
    if i == 0 or i == 1:
      continue

    # Open in O_RDWR | O_NONBLOCK to prevent blocking
    fd_in = os.open(fifo_in, os.O_RDWR | os.O_NONBLOCK)
    fd_out = os.open(fifo_out, os.O_RDWR | os.O_NONBLOCK)
    t = threading.Thread(
        target=_single_port_responder_loop,
        args=(i, fd_in, fd_out, stop_event),
        daemon=True)
    t.start()

  return qemu_args, stop_event


def _single_port_responder_loop(port_idx, fd_in, fd_out, stop_event):
  logging.info(f"[HVC Mock Port {port_idx}] Responder thread started.")
  buffer = b''
  try:
    while not stop_event.is_set():
      needed = 8 - len(buffer)
      if needed > 0:
        try:
          data = os.read(fd_out, needed)
          if data:
            buffer += data
        except BlockingIOError:
          pass

      if len(buffer) == 8:
        header_data = buffer
        buffer = b''

        cmd_and_is_resp, payload_sz = struct.unpack('<II', header_data)
        logging.info(f"[HVC Mock Port {port_idx}] Raw Header: {header_data.hex()}")
        cmd = cmd_and_is_resp & 0x7FFFFFFF
        logging.info(
            f"[HVC Mock Port {port_idx}] Request: cmd={cmd}, "
            f"is_resp={(cmd_and_is_resp >> 31) & 1}, payload_sz={payload_sz}")

        req_payload = b''
        if payload_sz > 0:
          while len(req_payload) < payload_sz and not stop_event.is_set():
            try:
              p_data = os.read(fd_out, payload_sz - len(req_payload))
              if p_data:
                req_payload += p_data
            except BlockingIOError:
              time.sleep(0.01)
          logging.info(f"[HVC Mock Port {port_idx}] Request payload: {req_payload}")

        if stop_event.is_set():
          break

        resp_payload = b'\x00'
        resp_header = struct.pack('<II', cmd | 0x80000000, len(resp_payload))
        os.write(fd_in, resp_header + resp_payload)
        logging.info(f"[HVC Mock Port {port_idx}] Responded \\x00 to cmd={cmd}")
      time.sleep(0.01)
  finally:
    os.close(fd_in)
    os.close(fd_out)
    logging.info(f"[HVC Mock Port {port_idx}] Responder thread stopped.")
