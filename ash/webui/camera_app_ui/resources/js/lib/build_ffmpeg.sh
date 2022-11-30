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

do_patch() {
  git apply -- "$1"
}

do_configure() {
  local args=(
    # emscripten toolchain
    --ar=emar
    --cc=emcc
    --cxx="em++"
    --ranlib=emranlib
    # generic architecture
    --arch=c
    --cpu=generic
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

  emconfigure ./configure "${args[@]}"
}

do_make() {
  emmake make -j "$(nproc)"
}

do_emcc() {
  cp ffmpeg ffmpeg.bc
  local args=(
    -s 'ASYNCIFY_IMPORTS=["wait_readable"]'
    -s 'EXTRA_EXPORTED_RUNTIME_METHODS=["FS"]'
    -s ALLOW_MEMORY_GROWTH=1
    -s ASYNCIFY
    -s EXIT_RUNTIME=1
    -s EXPORT_ES6=1
    -s MODULARIZE=1
    -s TOTAL_MEMORY=33554432
    -s USE_ES6_IMPORT_META=0
    -s WASM=1
    -v
    -Os
    --llvm-lto 3
    --js-library ./lib.js
    ffmpeg.bc
    -o
    ffmpeg.js
  )
  emcc "${args[@]}"
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

  # locate ffmpeg directory in chromium source tree
  local ffmpeg_dir
  ffmpeg_dir=$(realpath ../../../../../../third_party/ffmpeg)
  [[ -d "$ffmpeg_dir" ]] || die "ffmpeg not found"

  # copy ffmpeg/ into a temporary directory to patch and build
  local build_dir
  build_dir=$(mktemp -d -t ffmpeg_build_XXXXXX)
  cp --recursive --no-target-directory -- "$ffmpeg_dir" "$build_dir"

  pushd -- "$build_dir" > /dev/null

  do_patch "$patch_file"
  do_configure
  do_make
  do_emcc
  do_add_header

  popd

  cp --interactive -- "$build_dir"/ffmpeg.{js,wasm} .
}

main "$@"
