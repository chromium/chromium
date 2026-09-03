# Screen Recording on ChromeOS / Chromebook Skill - E2E Test Plan

## Prerequisites

**Read `SKILL.md` first** to understand device prerequisites (SSH credentials,
port forwarding, and `/usr/local/bin/ffmpeg` with `kmsgrab`).

______________________________________________________________________

## Test 1: Start Screen Recording (ChromeOS Device / cros DUT / Chromebook)

**Prompt:** "Start a screen recording on my ChromeOS device so I can capture
this UI bug." *(Alternative prompts: "Let's begin capturing video on my cros
DUT", "Start screen capture on my connected ChromeOS DUT")*

**Verify:**

- Agent connects to the ChromeOS / CrOS / Chromebook DUT via SSH.
- Agent executes the background `ffmpeg` recording command using
  `-f kmsgrab -i - -vf 'hwdownload,format=bgr0' -c:v msmpeg4v2`.
- Agent records to `/tmp/` and saves the PID to `/tmp/screen_record.pid` for
  clean shutdown.
- Agent confirms the process is running to the user.

______________________________________________________________________

## Test 2: Stop and Pull Screen Recording

**Prompt:** "I finished reproducing the issue on my Chromebook device. Please
stop the screen recording, pull the video to my machine, and convert it to MP4."
*(Alternative prompt: "Stop the recording on my cros DUT and save the video
locally on my workstation.")*

**Verify:**

- Agent sends `SIGINT` / `kill -INT` to the recording PID on the device.
- Agent copies the recorded video from the device to the host via `scp`.
- Agent transcodes to MP4 using host `ffmpeg`.
- Agent reports the local video filepath to the user.

______________________________________________________________________

## Test 3: Check Recording Status

**Prompt:** "Is screen recording currently running on my cros device?"

**Verify:**

- Agent queries the process list or PID file on the device via SSH.
- Agent accurately reports whether the recording is active.
