#!/bin/bash
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script updates sysroot-creator-*.sh with the timestamp of the latest
# snapshot from snapshot.debian.org.

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
ARCHIVE_URL="http://snapshot.debian.org/archive/debian"

# Use 9999-01-01 as the date so that we get a redirect to the page with the
# latest timestamp.
TIMESTAMP=$(curl -s "${ARCHIVE_URL}/99990101T000000Z/pool/" | \
  sed -n "s|.*${ARCHIVE_URL}/\([[:digit:]TZ]\+\)/pool/.*|\1|p" | head -n 1)

sed -i "s/ARCHIVE_TIMESTAMP=.*$/ARCHIVE_TIMESTAMP=${TIMESTAMP}/" \
  "${SCRIPT_DIR}"/sysroot-creator-*.sh
