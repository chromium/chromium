---
name: physical-chromebook-debugging
description: >-
  Build and deploy Chrome to a physical ChromeOS device, monitor logs, and take screenshots over SSH.
  Use when working with remote Chromebook testing hardware, deploying new builds
  of Chrome or ChromeOS, reading specific log files via SSH, or taking
  screenshots of a connected test endpoint.
---

# Physical Chromebook Debugging Skills

This skill outlines how to perform standard tasks on a remote or local ChromeOS
debugging device over SSH.

## 1. Building and Deploying Chrome

To build Chrome and deploy it to a remote ChromeOS device (or VM):

```bash
autoninja -C out_strongbad/Release chrome
./third_party/chromite/bin/deploy_chrome --build-dir=out_strongbad/Release --device=localhost:2222 --board=strongbad
```

> [!TIP] Replace `out_strongbad/Release`, `localhost:2222`, and `strongbad` with
> your actual configuration.

## 2. Reading Logs on the Device

Chrome logs are located in `/var/log/chrome/chrome`. View them via standard SSH
filtering:

```bash
ssh -p 2222 -i third_party/chromite/ssh_keys/testing_rsa root@localhost "grep -a 'SEARCH_TERM' /var/log/chrome/chrome"
```

## 3. Taking Screenshots

Capture a screenshot on the device and transfer it to your local machine with
`scp`:

1. Take screenshot on the device:

   ```bash
   ssh -p 2222 -i third_party/chromite/ssh_keys/testing_rsa root@localhost "screenshot /tmp/screenshot.png"
   ```

2. Copy to your local machine:

   ```bash
   scp -P 2222 -i third_party/chromite/ssh_keys/testing_rsa root@localhost:/tmp/screenshot.png /path/to/local/dir/
   ```

## 4. Emulating Input & ChromeVox Control

For simulating key presses or toggling the ChromeVox screen reader on a physical
Chromebook, please see the dedicated skill:
[chromevox-debugging-on-chromebook/SKILL.md](../chromevox-debugging-on-chromebook/SKILL.md)
