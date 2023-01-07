#!/bin/bash -p

# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Called as root after Keystone ticket promotion to change the owner, group,
# and permissions on the application.  The application bundle and its contents
# are set to owner root, group wheel, and to be writable only by root, but
# readable and executable (when appropriate) by everyone.
#
# Note that this script will be invoked with the real user ID set to the
# user's ID, but the effective user ID set to 0 (root).  bash -p is used on
# the first line to prevent bash from setting the effective user ID to the
# real user ID (dropping root privileges).
#
# WARNING: This script is NOT currently run when the Keystone ticket is
# promoted during application installation directly from the disk image,
# because the installation process itself handles the same permission fix-ups
# that this script normally would.

set -e

# This script runs as root, so be paranoid about things like ${PATH}.
export PATH="/usr/bin:/usr/sbin:/bin:/sbin"

# Output the pid to stdout before doing anything else.  See
# base/mac/authorization_util.h.
echo "${$}"

if [ ${#} -ne 1 ] ; then
  echo "usage: ${0} APP" >& 2
  exit 2
fi

APP="${1}"

# Make sure that APP is an absolute path and that it exists.
if [ -z "${APP}" ] || [ "${APP:0:1}" != "/" ] || [ ! -d "${APP}" ] ; then
  echo "${0}: must provide an absolute path naming an extant directory" >& 2
  exit 3
fi

OWNER_GROUP="root:wheel"
chown -Rh "${OWNER_GROUP}" "${APP}" >& /dev/null

CHMOD_MODE="a+rX,u+w,go-w"
chmod -R "${CHMOD_MODE}" "${APP}" >& /dev/null

# On the Mac, or at least on HFS+, symbolic link permissions are significant,
# but chmod -R and -h can't be used together.  Do another pass to fix the
# permissions on any symbolic links.
find "${APP}" -type l -exec chmod -h "${CHMOD_MODE}" {} + >& /dev/null

exit 0
