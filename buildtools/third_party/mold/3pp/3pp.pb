# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

create {
  source {
    git {
      # TODO(agrieve): Move back to rui314 if PRs are accepted:
      #    https://github.com/rui314/mold/pull/1590
      #    https://github.com/rui314/mold/pull/1599
      #    https://github.com/rui314/mold/pull/1600
      #    https://github.com/rui314/mold/pull/1601
      repo: "https://github.com/agrieve/mold.git"
      fixed_commit: "6861d4c75a99dc9a97ebdb80eaad50c5619041e2"
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
