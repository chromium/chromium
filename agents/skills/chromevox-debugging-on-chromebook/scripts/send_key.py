# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script to send a raw key event to a ChromeOS evdev input node.

Uses the standard evdev protocol to simulate a key press and release via raw
file descriptor writes.
"""

import argparse
import struct
import sys
import time
from typing import BinaryIO

# Standard evdev event types
EV_SYN = 0
EV_KEY = 1


def send_key(fd: BinaryIO, *, code: int, value: int) -> None:
    """Sends a raw evdev input event to the provided file descriptor.

  Args:
      fd: Open binary file object/descriptor for the input node.
      code: The integer evdev key code (e.g., 28 for ENTER).
      value: 1 for key press, 0 for key release.
  """
    # struct input_event: time (2 longs), type (short), code (short),
    # value (int)
    # On 64-bit Linux, time is usually 2 64-bit longs.
    event = struct.pack("llHHi", 0, 0, EV_KEY, code, value)
    fd.write(event)
    fd.flush()
    syn = struct.pack("llHHi", 0, 0, EV_SYN, 0, 0)
    fd.write(syn)
    fd.flush()


def main() -> None:
    """Parses command-line arguments and sends the key event."""
    parser = argparse.ArgumentParser(
        description="Send a raw key event to an evdev input node.")
    parser.add_argument(
        "device_node",
        type=str,
        help="The path to the evdev input node (e.g., /dev/input/event5).",
    )
    parser.add_argument(
        "key_code",
        type=int,
        help="The integer evdev key code to send.",
    )

    args = parser.parse_args()

    try:
        with open(args.device_node, "wb") as fd:
            send_key(fd, code=args.key_code, value=1)
            time.sleep(0.1)
            send_key(fd, code=args.key_code, value=0)
    except PermissionError:
        print(
            f"Error: Permission denied opening {args.device_node}. Must run as"
            " root.",
            file=sys.stderr,
        )
        sys.exit(1)


if __name__ == "__main__":
    main()
