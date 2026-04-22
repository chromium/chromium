---
name: chromevox-debugging-on-chromebook
description: >-
  Toggle ChromeVox screen reader on ChromeOS devices and send raw keyboard input events.
  Use when debugging accessibility features on an external ChromeOS device, needing to enable
  or disable ChromeVox, or sending specific keys to the system via SSH.
---

# ChromeVox Debugging on Chromebook

This skill provides instructions and scripts for toggling ChromeVox and
injecting key events into standard ChromeOS devices over SSH.

## 1. Toggling ChromeVox

You can run the bundled Python script on the targeted device to toggle ChromeVox
by simulating a `Ctrl+Alt+Z` keyboard shortcut.

Save or copy
`agents/skills/chromevox-debugging-on-chromebook/scripts/toggle_chromevox.py` to
`/tmp/toggle_chromevox.py` on the device. Then execute it via SSH:

```bash
ssh -p 2222 -i third_party/chromite/ssh_keys/testing_rsa root@localhost "python3 /tmp/toggle_chromevox.py /dev/input/event5"
```

> [!NOTE] Replace `/dev/input/event5` with the correct keyboard input device
> node. You can find the keyboard node on the device by running:
> `python3 -c "import evdev; [print(evdev.InputDevice(path).name, path) for path in evdev.list_devices()]"`

## 2. Sending Custom Key Events

If you need to inject specific keys into the system, save or copy
`agents/skills/chromevox-debugging-on-chromebook/scripts/send_key.py` to
`/tmp/send_key.py` on the device.

Execute it specifying the device node and key code (e.g., `44` for `KEY_Z`):

```bash
ssh -p 2222 -i third_party/chromite/ssh_keys/testing_rsa root@localhost "python3 /tmp/send_key.py /dev/input/event5 44"
```
