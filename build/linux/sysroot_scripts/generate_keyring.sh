#!/bin/bash
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -o nounset
set -o errexit

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

KEYS=(
    # Debian Archive Automatic Signing Key (11/bullseye)
    "73A4F27B8DD47936"
    # Debian Security Archive Automatic Signing Key (11/bullseye)
    "A48449044AAD5C5D"
    # Debian Stable Release Key (11/bullseye)
    "605C66F00D6C9793"
    # Debian Stable Release Key (10/buster)
    "DCC9EFBF77E11517"
    # Debian Archive Automatic Signing Key (10/buster)
    "DC30D7C23CBBABEE"
    # Debian Security Archive Automatic Signing Key (10/buster)
    "4DFAB270CAA96DFA"
    # Jessie Stable Release Key
    "CBF8D6FD518E17E1"
    # Debian Archive Automatic Signing Key (7.0/wheezy)
    "8B48AD6246925553"
    # Debian Archive Automatic Signing Key (8/jessie)
    "7638D0442B90D010"
    # Debian Security Archive Automatic Signing Key (8/jessie)
    "9D6D8F6BC857C906"
    # Debian Archive Automatic Signing Key (9/stretch)
    "E0B11894F66AEC98"
    # Debian Security Archive Automatic Signing Key (9/stretch)
    "EDA0D2388AE22BA9"
    # Debian Stable Release Key (9/stretch)
    "EF0F382A1A7B6500"
)

gpg --keyserver keyserver.ubuntu.com --recv-keys ${KEYS[@]}
gpg --output "${SCRIPT_DIR}/keyring.gpg" --export ${KEYS[@]}
