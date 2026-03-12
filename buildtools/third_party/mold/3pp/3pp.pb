# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
create {
  source {
    script { name: "fetch.py" }
    unpack_archive: true
  }
}

upload {
  pkg_prefix: "chromium/buildtools/third_party/mold"
  universal: true
}
