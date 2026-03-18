#!/usr/bin/env bash

# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

check_dep() {
  eval "$1" >/dev/null 2>&1
  if [ $? -ne 0 ]; then
    echo >&2 "This script requires $2."
    echo >&2 "Have you tried $3?"
    exit 1
  fi
}

if [ "$#" -eq 0 ]; then
    echo "Error: No arguments provided. Usage: $0 <path to polymer component>.ts" >&2
    exit 1
fi

set -e

check_dep "which npm" "npm" "visiting https://nodejs.org/en/"

echo "Performing initial migration steps for $1"

npm install --prefix ui/webui/resources/tools/codemods/ --no-bin-links \
    --no-fund --ignore-scripts --omit=dev --omit=optional

python3 ui/webui/resources/tools/codemods/lit_migration.py \
    --file "$1"

rm "${1%.ts}.html"

echo "Migration done"
