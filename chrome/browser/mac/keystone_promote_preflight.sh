#!/bin/bash -p

# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Called as root before Keystone ticket promotion to ensure a suitable
# environment for Keystone installation.  Ultimately, these features should be
# integrated directly into the Keystone installation.
#
# If the two branding paths are given, then the branding information is also
# copied and the permissions on the system branding file are set to be owned by
# root, but readable by anyone.
#
# Note that this script will be invoked with the real user ID set to the
# user's ID, but the effective user ID set to 0 (root).  bash -p is used on
# the first line to prevent bash from setting the effective user ID to the
# real user ID (dropping root privileges).

set -e

# This script runs as root, so be paranoid about things like ${PATH}.
export PATH="/usr/bin:/usr/sbin:/bin:/sbin"

# Output the pid to stdout before doing anything else.  See
# base/mac/authorization_util.h.
echo "${$}"

if [ ${#} -ne 0 ] && [ ${#} -ne 2 ] ; then
  echo "usage: ${0} [USER_BRAND SYSTEM_BRAND]" >& 2
  exit 2
fi

if [ ${#} -eq 2 ] ; then
  USER_BRAND="${1}"
  SYSTEM_BRAND="${2}"

  # Make sure that USER_BRAND is an absolute path and that it exists.
  if [ -z "${USER_BRAND}" ] || \
     [ "${USER_BRAND:0:1}" != "/" ] || \
     [ ! -f "${USER_BRAND}" ] ; then
    echo "${0}: must provide an absolute path naming an existing user file" >& 2
    exit 3
  fi

  # Make sure that SYSTEM_BRAND is an absolute path.
  if [ -z "${SYSTEM_BRAND}" ] || [ "${SYSTEM_BRAND:0:1}" != "/" ] ; then
    echo "${0}: must provide an absolute path naming a system file" >& 2
    exit 4
  fi

  # Make sure the directory for the system brand file exists.
  SYSTEM_BRAND_DIR=$(dirname "${SYSTEM_BRAND}")
  if [ ! -e "${SYSTEM_BRAND_DIR}" ] ; then
    mkdir -p "${SYSTEM_BRAND_DIR}"
    # Permissions on this directory will be fixed up at the end of this script.
  fi

  # Copy the brand file
  cp "${USER_BRAND}" "${SYSTEM_BRAND}" >& /dev/null

  # Ensure the right ownership and permissions
  chown "root:wheel" "${SYSTEM_BRAND}" >& /dev/null
  chmod "a+r,u+w,go-w" "${SYSTEM_BRAND}" >& /dev/null

fi

exit 0
