#!/bin/bash -p

# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Called by the application to install in a new location.  Generally, this
# means that the application is running from a disk image and wants to be
# copied to /Applications.  The application, when running from the disk image,
# will call this script to perform the copy.
#
# This script will be run as root if the application determines that it would
# not otherwise have permission to perform the copy.
#
# When running as root, this script will be invoked with the real user ID set
# to the user's ID, but the effective user ID set to 0 (root).  bash -p is
# used on the first line to prevent bash from setting the effective user ID to
# the real user ID (dropping root privileges).

set -e

# This script may run as root, so be paranoid about things like ${PATH}.
export PATH="/usr/bin:/usr/sbin:/bin:/sbin"

# If running as root, output the pid to stdout before doing anything else.
# See chrome/browser/mac/authorization_util.h.
if [ ${EUID} -eq 0 ] ; then
  echo "${$}"
fi

if [ ${#} -ne 2 ] ; then
  echo "usage: ${0} SRC DEST" >& 2
  exit 2
fi

SRC=${1}
DEST=${2}

# Make sure that SRC is an absolute path and that it exists.
if [ -z "${SRC}" ] || [ "${SRC:0:1}" != "/" ] || [ ! -d "${SRC}" ] ; then
  echo "${0}: source ${SRC} sanity check failed" >& 2
  exit 3
fi

# Make sure that DEST is an absolute path and that it doesn't yet exist.
if [ -z "${DEST}" ] || [ "${DEST:0:1}" != "/" ] || [ -e "${DEST}" ] ; then
  echo "${0}: destination ${DEST} sanity check failed" >& 2
  exit 4
fi

# Do the copy.
rsync --links --recursive --perms --times "${SRC}/" "${DEST}"

# The remaining steps are not considered critical.
set +e

# Notify LaunchServices.
CORESERVICES="/System/Library/Frameworks/CoreServices.framework"
LAUNCHSERVICES="${CORESERVICES}/Frameworks/LaunchServices.framework"
LSREGISTER="${LAUNCHSERVICES}/Support/lsregister"
"${LSREGISTER}" -f "${DEST}"

# If this script is not running as root and the application is installed
# somewhere under /Applications, try to make it writable by all admin users.
# This will allow other admin users to update the application from their own
# user Keystone instances even if the Keystone ticket is not promoted to
# system level.
#
# If the script is not running as root and the application is not installed
# under /Applications, it might not be in a system-wide location, and it
# probably won't be something that other users on the system are running, so
# err on the side of safety and don't make it group-writable.
#
# If this script is running as root, a Keystone ticket promotion is expected,
# and future updates can be expected to be applied as root, so
# admin-writeability is not a concern.  Set the entire thing to be owned by
# root in that case, regardless of where it's installed, and drop any group
# and other write permission.
#
# If this script is running as a user that is not a member of the admin group,
# the chgrp operation will not succeed.  Tolerate that case, because it's
# better than the alternative, which is to make the application
# world-writable.
CHMOD_MODE="a+rX,u+w,go-w"
if [ ${EUID} -ne 0 ] ; then
  if [ "${DEST:0:14}" = "/Applications/" ] &&
     chgrp -Rh admin "${DEST}" >& /dev/null ; then
    CHMOD_MODE="a+rX,ug+w,o-w"
  fi
else
  chown -Rh root:wheel "${DEST}" >& /dev/null
fi

chmod -R "${CHMOD_MODE}" "${DEST}" >& /dev/null

# On the Mac, or at least on HFS+, symbolic link permissions are significant,
# but chmod -R and -h can't be used together.  Do another pass to fix the
# permissions on any symbolic links.
find "${DEST}" -type l -exec chmod -h "${CHMOD_MODE}" {} + >& /dev/null

# Because this script is launched by the application itself, the installation
# process inherits the quarantine bit (LSFileQuarantineEnabled).  Any files or
# directories created during the update will be quarantined in that case,
# which may cause Launch Services to display quarantine UI.  That's bad,
# especially if it happens when the outer .app launches a quarantined inner
# helper.  Since the user approved the application launch if quarantined, it
# it can be assumed that the installed copy should not be quarantined.  Use
# xattr to drop the quarantine attribute.
QUARANTINE_ATTR=com.apple.quarantine
xattr -d -r "${QUARANTINE_ATTR}" "${DEST}" >& /dev/null

# Great success!
exit 0
