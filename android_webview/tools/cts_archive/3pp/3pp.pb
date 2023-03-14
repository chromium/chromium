# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

create {
  source {
    script {
      name: "fetch.py"
      verification_timeout: "30m"
    }
  }
  build {}
}

upload {
  pkg_prefix: "chromium/android_webview/tools"
  universal: true
}
