# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Pure Python implementation of Android Sparse Image decoder (simg2img).

NOTE: This is a temporary solution before we can find a better way to include
the simg2img C++ binary. It's slower but works without host binary dependencies.
"""

import logging
import os
import struct


def is_sparse_image(file_path):
    """Checks if a file is an Android Sparse Image by reading its magic bytes."""
    if not os.path.exists(file_path):
        return False
    with open(file_path, 'rb') as f:
        header = f.read(4)
        return header == b'\x3a\xff\x26\xed'


def unsparse_image(sparse_path, raw_path):
    """Pure Python implementation of simg2img."""
    logging.info(f"Unsparsing {sparse_path} using pure Python decoder...")
    with open(sparse_path, 'rb') as in_f, open(raw_path, 'wb') as out_f:
        header_data = in_f.read(28)
        if len(header_data) < 28:
            raise ValueError("Invalid sparse image: header too short")
        (
            magic,
            major,
            minor,
            file_hdr_sz,
            chunk_hdr_sz,
            blk_sz,
            total_blks,
            total_chunks,
            _,
        ) = struct.unpack('<IHHHHIIII', header_data)

        if magic != 0xED26FF3A:
            raise ValueError(f"Invalid sparse image magic: {hex(magic)}")

        if file_hdr_sz > 28:
            in_f.seek(file_hdr_sz)

        for _ in range(total_chunks):
            chunk_header = in_f.read(12)
            if len(chunk_header) < 12:
                break
            chunk_type, _, chunk_sz, total_sz = struct.unpack(
                '<HHII', chunk_header
            )
            data_sz = chunk_sz * blk_sz

            if chunk_type == 0xCAC1:  # RAW
                remaining = data_sz
                while remaining > 0:
                    chunk_bytes = in_f.read(min(remaining, 1024 * 1024))
                    out_f.write(chunk_bytes)
                    remaining -= len(chunk_bytes)
            elif chunk_type == 0xCAC2:  # FILL
                fill_val = in_f.read(4)
                zero_buf = fill_val * (1024 * 256)
                blocks = data_sz // len(zero_buf)
                remainder = data_sz % len(zero_buf)
                for _ in range(blocks):
                    out_f.write(zero_buf)
                if remainder:
                    out_f.write(fill_val * (remainder // 4))
            elif chunk_type == 0xCAC3:  # DONT CARE
                out_f.seek(data_sz, os.SEEK_CUR)
            elif chunk_type == 0xCAC4:  # CRC32
                in_f.read(4)
            else:
                raise ValueError(
                    f"Unknown sparse chunk type: {hex(chunk_type)}"
                )


def unsparse_in_place(image_path):
    """Converts image_path in-place if it is an Android sparse image."""
    if not is_sparse_image(image_path):
        logging.info(
            f"{image_path} is already raw/unsparse. Skipping simg2img."
        )
        return

    logging.info(f"Unsparsing {image_path}...")
    raw_path = image_path + '.raw'
    unsparse_image(image_path, raw_path)
    os.replace(raw_path, image_path)
