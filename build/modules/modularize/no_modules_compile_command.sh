#!/bin/bash -eu

# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

if [[ $# -lt 1 ]]; then
    echo "Usage: ./no_modules_compile_command.sh <gn output directory> <os (optional, default linux)>"
    exit 1
fi

src="../../build/modules/modularize/no_modules.cc"

# Ensure all the dependencies are present on disk.
autoninja -C "$1" -offline "${src}^" 1>&2

# Outputs a generic command to compile an arbitrary cc file.
siso query commands -C "$1" "${src}^" | \
  tail -1 | \
  sed -E 's~ -MMD -MF [^ ]* ~ ~' | \
  sed 's~ /showIncludes:user ~ ~' | \
  sed -E "s~ (-o |/Fo)[^ ]* ~ ~" | \
  sed 's~ /TP ~ ~' | \
  sed "s~ ${src} ~ ~g" | \
  sed 's~  ~ ~g' | \
  tr -d '\n'

# This specifically needs to go at the end.
# Otherwise Windows attempts to compile to /dev/null.obj
echo " -o /dev/null"
