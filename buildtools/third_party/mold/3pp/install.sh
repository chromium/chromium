#!/bin/bash
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -euxo pipefail

PREFIX="$1"
DEPS_PREFIX="$2"

# 1. Prepare build directory
mkdir -p build
cd build

# 2. Configure
# We use CMAKE_INSTALL_PREFIX to install to the 3pp prefix.
# We also set CMAKE_BUILD_TYPE to Release.
# We disable testing to save time and dependencies.
cmake .. \
  -DCMAKE_INSTALL_PREFIX="${PREFIX}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=OFF

# 3. Build
cmake --build . --parallel "$(nproc)"

# 4. Install
# We only want the 'mold' binary, directly in the PREFIX (not in bin/ subdirectory).
mkdir -p temp_install
cmake --install . --prefix temp_install
cp temp_install/bin/mold "$PREFIX/"

