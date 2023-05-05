#!/bin/bash -e

# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

exec "$(cd $(dirname $0) && pwd)/install-build-deps.py" "$@"
