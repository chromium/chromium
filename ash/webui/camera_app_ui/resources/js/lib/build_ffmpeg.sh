#!/bin/bash

# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# We are using unquoted '=' in array as intended.
# shellcheck disable=SC2191

set -ex

die() {
  echo -e "\e[1;31merror: $*\e[0m" 1>&2
  exit 1
}

do_copy_ffmpeg() {
  # locate ffmpeg directory in chromium source tree
  local ffmpeg_dir
  ffmpeg_dir=$(realpath ../../../../../../third_party/ffmpeg)
  [[ -d "$ffmpeg_dir" ]] || die "ffmpeg not found"

  # checkout specific commit to keep code base consistent to the patch
  # TODO(b/199980849): Fix pthread issue to be able to use latest FFMpeg update.
  # TODO(b/199980849): Remove video_size patch once the PR is sent upstream.
  local build_commit=90810bb
  (
    cd $ffmpeg_dir;
    git archive --format=tar "$build_commit" | (cd "$1" && tar xf -)
  )
}

do_patch() {
  git apply -- "$1"
}

do_configure() {
  local WASM_LDFLAGS=(
    -s ASYNCIFY_IMPORTS='["wait_readable"]'
    -s EXTRA_EXPORTED_RUNTIME_METHODS='["FS"]'
    -s ALLOW_MEMORY_GROWTH=1
    -s ASYNCIFY
    -s EXIT_RUNTIME=1
    -s ENVIRONMENT=web
    -s EXPORT_ES6=1
    -s MODULARIZE=1
    -s TOTAL_MEMORY=33554432
    -s USE_ES6_IMPORT_META=0
    -s WASM=1
    -v
    -Os
    --llvm-lto 3
    --js-library ./lib.js
  )
  local args=(
    # emscripten toolchain
    --ar=emar
    --cc=emcc
    --cxx="em++"
    --ranlib=emranlib
    # generic architecture
    --arch=c
    --cpu=generic
    --extra-ldflags="$LDFLAGS"
    --disable-asm
    --disable-stripping
    --enable-cross-compile
    --target-os=none
    # smaller binary
    --disable-all
    --disable-autodetect
    --disable-debug
    --disable-everything
    --disable-iconv
    --disable-network
    --disable-pthreads
    --disable-runtime-cpudetect
    --enable-lto
    --enable-small
    # enable selective features
    --enable-avcodec
    --enable-avfilter
    --enable-avformat
    --enable-ffmpeg
    --enable-swresample
    --enable-swscale
    --enable-protocol=file
    --enable-protocol=pipe
    --enable-demuxer=h264
    --enable-demuxer=matroska
    --enable-demuxer=rawvideo
    --enable-muxer=gif
    --enable-muxer=mov
    --enable-muxer=mp4
    --enable-parser=h264
    --enable-decoder=pcm_f32le
    --enable-decoder=rawvideo
    --enable-encoder=aac
    --enable-encoder=gif
    --enable-filter=aresample
    --enable-filter=scale
    --enable-bsf=extract_extradata
  )

  LDFLAGS="${WASM_LDFLAGS[@]}" emconfigure ./configure "${args[@]}"
}

do_make() {
  emmake make -j "$(nproc)"
}

do_cleanup_name() {
  mv ffmpeg_g.wasm ffmpeg.wasm
  mv ffmpeg_g ffmpeg.js
  sed -i -e 's/ffmpeg_g.wasm/ffmpeg.wasm/g' ffmpeg.js
}

do_add_header() {
  mv ffmpeg.js ffmpeg.orig.js
  cat <<'EOF' > ffmpeg.js
// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
/* eslint-disable */
// @ts-nocheck
EOF
  cat ffmpeg.orig.js >> ffmpeg.js
  echo >> ffmpeg.js
}

main() {
  [[ -z "$EMSDK" ]] && die "emsdk is not setup properly"

  # change working directory to the script location
  cd -- "$(dirname -- "$(realpath -- "${BASH_SOURCE[0]}")")"

  # locate patch file
  local patch_file
  patch_file=$(realpath ffmpeg.patch)
  [[ -f "$patch_file" ]] || die "patch not found"

  # copy ffmpeg/ into a temporary directory to patch and build
  local build_dir
  build_dir=$(mktemp -d -t ffmpeg_build_XXXXXX)
  do_copy_ffmpeg "$build_dir"
  pushd -- "$build_dir" > /dev/null
  do_patch "$patch_file"

  # configure + build
  do_configure
  do_make
  do_cleanup_name
  do_add_header

  popd

  cp --interactive -- "$build_dir"/ffmpeg.{js,wasm} .
}

main "$@"
