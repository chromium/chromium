# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

create {
  source {
    git {
      # TODO(agrieve): Move back to rui314 if PR is accepted:
      #    https://github.com/rui314/mold/pull/1590
      repo: "https://github.com/agrieve/mold.git"
      fixed_commit: "b3816a58f2e16f12594a483b5451315f17488f20"
    }
  }

  build {
    install: "install.sh"
    external_tool: "infra/3pp/tools/cmake/${platform}@3@3.31.12.chromium.8"
  }
}

upload {
  pkg_prefix: "chromium/buildtools/third_party/mold"
  universal: true
}
