#!/bin/bash

# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Script to install everything needed to build chromium on android, including
# items requiring sudo privileges.
# See https://www.chromium.org/developers/how-tos/android-build-instructions

args="$@"

if ! uname -m | egrep -q "i686|x86_64"; then
  echo "Only x86 architectures are currently supported" >&2
  exit
fi

# Exit if any commands fail.
set -e

lsb_release=$(lsb_release --codename --short)

# Install first the default Linux build deps.
"$(dirname "${BASH_SOURCE[0]}")/install-build-deps.sh" \
  --no-syms --lib32 --no-arm --no-chromeos-fonts --no-nacl --no-prompt "${args}"

# Fix deps
sudo apt-get -f install

# common
sudo apt-get -y install lib32z1 lighttpd python-pexpect xvfb x11-utils

# Some binaries in the Android SDK require 32-bit libraries on the host.
# See https://developer.android.com/sdk/installing/index.html?pkg=tools
sudo apt-get -y install libncurses5:i386 libstdc++6:i386 zlib1g:i386

# Required for apk-patch-size-estimator
sudo apt-get -y install bsdiff

# Do our own error handling for java.
set +e

# First arg is either "java" or "javac", second is Java version
function IsJavaInstalled() {
  $1 -version 2>&1 | grep -q "$2"
}

# First arg is Java release, second argument is string showing version
# (Java > 8 is not reporting version as 1.x)
function CheckOrInstallJava() {
  if ! (IsJavaInstalled java $2 && IsJavaInstalled javac $2); then
    sudo apt-get -y install openjdk-$1-jre openjdk-$1-jdk
  fi

  # There can be several reasons why java is not default despite being installed.
  # Just show an error and exit.
  if ! (IsJavaInstalled java $2 && IsJavaInstalled javac $2); then
    echo
    echo "Automatic java installation failed."
    echo '`java -version` reports:'
    java -version
    echo
    echo '`javac -version` reports:'
    javac -version
    echo
    echo "Please ensure that JDK $1 is installed and resolves first in your PATH."
    echo -n '`which java` reports: '
    which java
    echo
    echo -n '`which javac` reports: '
    which javac
    echo
    echo
    echo "You might also try running:"
    if [[ "$lsb_release" == "disco" && "$1" == "8" ]]; then
       # Propose repository from latest LTS with Java 8 (Ubuntu 18.04 LTS) in Ubuntu 19.04 without Java 8
       echo "    sudo add-apt-repository -u -y 'deb http://archive.ubuntu.com/ubuntu bionic-security universe'"
       echo "    sudo apt-get install openjdk-8-jdk"
       echo
       echo "    OR"
       echo
    fi
    echo "    sudo update-java-alternatives -s java-1.$1.0-openjdk-amd64"
    exit 1
  fi
}

if [[ "$lsb_release" == "disco" ]]; then
  # TODO(marcin@mwiacek.com): Investigate what needs to be changed to support Java 11.
  #
  # Currently known:
  # 1. changing version in internal_rules.gni (it's hardcoded to 1.8)
  # 2. replacing -Xbootclasspath/p: with -Xbootclasspath/a: (?)
  # Issue during compiling chrome_public_apk:
  # Exception in thread "main" java.lang.InternalError:
  # Cannot find requested resource bundle for locale en_US
  #
  # CheckOrInstallJava 11 "11."
  CheckOrInstallJava 8 "1.8"
else
  CheckOrInstallJava 8 "1.8"
fi

echo "install-build-deps-android.sh complete."
