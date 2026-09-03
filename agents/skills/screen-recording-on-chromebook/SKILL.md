---
name: screen-recording-on-chromebook
description: >-
  Records screen video on ChromeOS devices, Chromebooks, and CrOS DUTs over SSH using ffmpeg KMS grab, and transcodes the video to MP4 on the host workstation. Use when recording the screen of a Chromebook, ChromeOS device, cros DUT, or CrOS test hardware, capturing visual bug reproductions or feature demonstrations on ChromeOS/cros, starting/stopping background screen capture on a Chromebook or ChromeOS DUT, checking recording status on a cros device, or pulling and transcoding recorded video files from a DUT. Don't use for Android devices (use recording-device-screens or adb-uiautomator), desktop Linux/Mac screen capture, taking static screenshots (use physical-chromebook-debugging), uploading videos (use screencast skill), or browser-only DOM recordings.
---

# Screen Recording on ChromeOS (Chromebook / CrOS DUT)

Capture screen recordings directly from the display framebuffer of any ChromeOS
device, Chromebook, or CrOS DUT over SSH and convert the video on the host
workstation.

## Prerequisites & Device Setup

- **SSH Access**: Ensure you can connect to the ChromeOS / CrOS DUT as `root`
  (e.g. `ssh -p <port> -i <key> root@<host>`).
  - Default test key: `third_party/chromite/ssh_keys/testing_rsa` (or
    `~/.ssh/testing_rsa`).
  - Default port for local bridged/forwarded DUTs is typically `2222` (or
    standard `22` for direct network DUTs).
- **FFmpeg on DUT**: ChromeOS test images include `/usr/local/bin/ffmpeg` with
  KMS grab (`kmsgrab`) support enabled. Modern ChromeOS uses DRM/KMS; `/dev/fb0`
  does not exist.

______________________________________________________________________

## 1. Start Screen Recording

To start capturing the screen in the background on the ChromeOS / Chromebook /
CrOS DUT:

```bash
ssh -i third_party/chromite/ssh_keys/testing_rsa -o StrictHostKeyChecking=no -p {PORT} root@{DUT_HOST}   "rm -f /tmp/dut_recording.avi /tmp/dut_recording.log /tmp/screen_record.pid;    nohup ffmpeg -y -f kmsgrab -framerate 30 -i - -vf 'hwdownload,format=bgr0' -c:v msmpeg4v2 -pix_fmt yuv420p -q:v 3 /tmp/dut_recording.avi > /tmp/dut_recording.log 2>&1 &    echo \$! > /tmp/screen_record.pid;    sleep 1;    ps aux | grep -v grep | grep ffmpeg"
```

> [!NOTE] **Key Options & Performance Tuning:**
>
> - **Framerate (`-framerate 30`)**: Defaults to 30 FPS to reduce CPU
>   utilization and avoid thermal throttling or UI stutter on
>   resource-constrained Chromebooks during reproduction. Lower to
>   `-framerate 15` on low-end hardware, or raise to `-framerate 60` for fast
>   animation testing.
> - **Pixel Format (`-pix_fmt yuv420p`)**: Explicitly instructs FFmpeg to
>   convert the `bgr0` KMS buffer directly into YUV 4:2:0 for the `msmpeg4v2`
>   encoder, avoiding filtergraph auto-negotiation overhead.
> - **Codec & Container (`-c:v msmpeg4v2` + AVI)**: Available across ARM and x86
>   ChromeOS test builds without hardware encoder dependencies, and flushes
>   gracefully on `SIGINT`.

______________________________________________________________________

## 2. Check Recording Status

To verify whether recording is actively running on the ChromeOS device /
Chromebook DUT:

```bash
ssh -i third_party/chromite/ssh_keys/testing_rsa -o StrictHostKeyChecking=no -p {PORT} root@{DUT_HOST}   "if [ -f /tmp/screen_record.pid ] && ps -p \$(cat /tmp/screen_record.pid) > /dev/null 2>&1; then      echo 'Recording is RUNNING (PID: '\$(cat /tmp/screen_record.pid)')';    else      echo 'Recording is NOT running';    fi"
```

______________________________________________________________________

## 3. Stop Screen Recording

Send `SIGINT` to the ffmpeg process so it cleanly finalizes the video container
and index:

```bash
ssh -i third_party/chromite/ssh_keys/testing_rsa -o StrictHostKeyChecking=no -p {PORT} root@{DUT_HOST}   "if [ -f /tmp/screen_record.pid ]; then      kill -INT \$(cat /tmp/screen_record.pid) 2>/dev/null || pkill -INT -f 'ffmpeg.*dut_recording';      sleep 2;    fi;    ls -lh /tmp/dut_recording.avi"
```

______________________________________________________________________

## 4. Pull and Transcode Video to MP4

Copy the recording from the ChromeOS DUT to the host workstation and convert it
to a standard H.264 MP4:

```bash
mkdir -p /tmp/recordings

# 1. Pull video from ChromeOS DUT
scp -i third_party/chromite/ssh_keys/testing_rsa -o StrictHostKeyChecking=no -P {PORT}   root@{DUT_HOST}:/tmp/dut_recording.avi /tmp/recordings/dut_recording.avi

# 2. Transcode to high-quality compressed MP4 on host
ffmpeg -i /tmp/recordings/dut_recording.avi -c:v libx264 -preset fast -pix_fmt yuv420p -y /tmp/recordings/dut_recording.mp4
```

The resulting file `/tmp/recordings/dut_recording.mp4` is ready for viewing,
attaching to bugs, or passing to separate upload tools.

______________________________________________________________________

## Troubleshooting & Common Pitfalls

| Issue                                             | Cause                                                                            | Solution                                                                                  |
| :------------------------------------------------ | :------------------------------------------------------------------------------- | :---------------------------------------------------------------------------------------- |
| `Could not open framebuffer device '/dev/fb0'`    | Modern ChromeOS uses DRM/KMS instead of legacy Linux fbdev.                      | Use `-f kmsgrab -i - -vf 'hwdownload,format=bgr0'` instead of `-f fbdev`.                 |
| High CPU usage or DUT thermal throttling          | Unconstrained default framerate (60 FPS) capturing at full display refresh rate. | Add `-framerate 30` or `-framerate 15` as an input option to `kmsgrab`.                   |
| `Could not find a valid device` for `vp8_v4l2m2m` | Hardware v4l2 mem2mem encoders vary per SoC.                                     | Use `-c:v msmpeg4v2 -pix_fmt yuv420p` which reliably software-encodes across ARM and x86. |
| Corrupted or unplayable output video              | `kill -9` (`SIGKILL`) was used, preventing ffmpeg from writing file headers.     | Always terminate with `kill -INT` (`SIGINT`) and sleep 1-2s for graceful flush.           |
| SSH Permission Denied / Read-only rootfs          | Trying to write to rootfs or missing test key.                                   | Always record to `/tmp/` (tmpfs is always writable) and specify `-i testing_rsa`.         |
