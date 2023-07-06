#!/bin/bash
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -ex

: "${PORT:=10224}"

# This launch a chrome instance that is suitable for local CCA development.
#
# --user-data-dir to initialize a new profile so Chrome won't reuse the
# existing process that doesn't have the fake VCD flag. We use a fixed path
# here so subsequent run will be faster and have local storage persisted.
#
# --use-fake-device-for-media-stream for using fake VCD. We specify FPS
# manually since the default FPS is lower than what CCA need (24 fps).
#
# --use-fake-ui-for-media-stream to not show the camera permission popup.
#
# --test-type to disable the annoying "Using unsupported flags" alert.
#
# --no-default-browser-check to disable the "set default browser" tooltip.
#
# --no-first-run to skip the dialog when the profile is first created.
google-chrome \
  --user-data-dir=/tmp/cca-dev \
  --use-fake-device-for-media-stream=fps=30 \
  --use-fake-ui-for-media-stream \
  --test-type \
  --no-default-browser-check \
  --no-first-run \
  "http://localhost:${PORT}"
