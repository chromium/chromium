# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script to toggle the ChromeVox screen reader on ChromeOS via evdev.

Simulates the standard Ctrl+Alt+Z keyboard shortcut to switch ChromeVox on or
off.
"""

import argparse
import struct
import sys
import time
from typing import BinaryIO

# Standard evdev event types
EV_SYN = 0
EV_KEY = 1

# Standard key codes for ChromeVox toggle
KEY_LEFTCTRL = 29
KEY_LEFTALT = 56
KEY_Z = 44


def send_key(fd: BinaryIO, *, code: int, value: int) -> None:
    """Sends a raw evdev input event to the provided file descriptor.

  Args:
      fd: Open binary file object/descriptor for the keyboard input node.
      code: The integer evdev key code.
      value: 1 for key press, 0 for key release.
  """
    # struct input_event: time (2 longs), type (short), code (short),
    # value (int)
    # On 64-bit Linux, time is usually 2 64-bit longs.
    event = struct.pack("llHHi", 0, 0, EV_KEY, code, value)
    fd.write(event)
    fd.flush()
    # Send SYN
    syn = struct.pack("llHHi", 0, 0, EV_SYN, 0, 0)
    fd.write(syn)
    fd.flush()


def toggle_chromevox(fd: BinaryIO) -> None:
    """Injects the Ctrl+Alt+Z key sequence to toggle ChromeVox.

  Args:
      fd: Open binary file object/descriptor for the keyboard input node.
  """
    # Press Ctrl+Alt+Z
    send_key(fd, code=KEY_LEFTCTRL, value=1)
    send_key(fd, code=KEY_LEFTALT, value=1)
    send_key(fd, code=KEY_Z, value=1)
    time.sleep(0.1)
    # Release in reverse order
    send_key(fd, code=KEY_Z, value=0)
    send_key(fd, code=KEY_LEFTALT, value=0)
    send_key(fd, code=KEY_LEFTCTRL, value=0)


def main() -> None:
    """Parses command-line arguments and toggles ChromeVox."""
    parser = argparse.ArgumentParser(
        description="Toggle ChromeVox screen reader via evdev input injection."
    )
    parser.add_argument(
        "device_node",
        type=str,
        help="The path to the keyboard input node (e.g., /dev/input/event5).",
    )

    args = parser.parse_args()

    try:
        with open(args.device_node, "wb") as fd:
            toggle_chromevox(fd)
    except PermissionError:
        print(
            f"Error: Permission denied opening {args.device_node}. Must run as"
            " root.",
            file=sys.stderr,
        )
        sys.exit(1)


if __name__ == "__main__":
    main()
