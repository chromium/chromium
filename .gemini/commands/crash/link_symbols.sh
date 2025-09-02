#!/bin/bash
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

if [ -z "$1" ]; then
  echo "Usage: $0 <debug_information_dir> <minidump_file>"
  exit 1
fi

if [ -z "$2" ]; then
  echo "Usage: $0 <debug_information_dir> <minidump_file>"
  exit 1
fi

DEBUG_DIR=`realpath $1`
MINIDUMP_FILE=`realpath $2`
SYMBOLS_DIR="lldb-symbols"

# Create a directory for the symbols and navigate into it.
mkdir -p "$SYMBOLS_DIR" && cd "$SYMBOLS_DIR"

# Process the minidump to extract module paths and create symlinks.
/google/bin/releases/crash/minidump_dump $MINIDUMP_FILE |
  grep '(code_file)' |
  awk -F'"' '{print $2}' |
  while read -r module_path; do
    # Get the base name of the module file (e.g., "libc.so.6").
    symlink_name=${module_path##*/}

    # Find the first matching debug file with or without .debug extension
    debug_file=$(find "${DEBUG_DIR}" -type f \( -name "${symlink_name}.debug" \
        -o -name "${symlink_name}" \) -print -quit)

    # If a debug file was found, create a symbolic link to it.
    if [[ -n "$debug_file" ]]; then
      ln -s "$debug_file" "$symlink_name"
    fi

    # Also try linking a .dwp file if present.
    debug_file=$(find "${DEBUG_DIR}" -name "${symlink_name}.dwp" -print -quit)

    if [[ -n "$debug_file" ]]; then
      ln -s "$debug_file" "$symlink_name.dwp"
    fi
  done

# Return to the original directory.
cd ..
