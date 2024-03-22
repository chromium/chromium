#!/usr/bin/env python3
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Returns a timestamp that approximates the build date.

build_type impacts the timestamp generated, both relative to the date of the
last recent commit:
- default: the build date is set to the most recent first Sunday of a month at
  5:00am. The reason is that it is a time where invalidating the build cache
  shouldn't have major repercussions (due to lower load).
- official: the build date is set to the time of the most recent commit.
Either way, it is guaranteed to be in the past and always in UTC.
"""

# The requirements for the timestamp:
# (1) for the purposes of continuous integration, longer duration
#     between cache invalidation is better, but >=1mo is preferable.
# (2) for security purposes, timebombs would ideally be as close to
#     the actual time of the build as possible. It must be in the past.
# (3) HSTS certificate pinning is valid for 70 days. To make CI builds enforce
#     HTST pinning, <=1mo is preferable.
#
# On Windows, the timestamp is also written in the PE/COFF file header of
# executables of dlls.  That timestamp and the executable's file size are
# the only two pieces of information that identify a given executable on
# the symbol server, so rarely changing timestamps can cause conflicts there
# as well. We only upload symbols for official builds to the symbol server.


import argparse
import calendar
import datetime
import doctest
import os
import sys


THIS_DIR = os.path.abspath(os.path.dirname(__file__))


def GetFirstSundayOfMonth(year, month):
  """Returns the first sunday of the given month of the given year.

  >>> GetFirstSundayOfMonth(2016, 2)
  7
  >>> GetFirstSundayOfMonth(2016, 3)
  6
  >>> GetFirstSundayOfMonth(2000, 1)
  2
  """
  weeks = calendar.Calendar().monthdays2calendar(year, month)
  # Return the first day in the first week that is a Sunday.
  return [date_day[0] for date_day in weeks[0] if date_day[1] == 6][0]


def GetUnofficialBuildDate(build_date):
  """Gets the approximate build date given the specific build type.

  >>> GetUnofficialBuildDate(datetime.datetime(2016, 2, 6, 1, 2, 3))
  datetime.datetime(2016, 1, 3, 5, 0)
  >>> GetUnofficialBuildDate(datetime.datetime(2016, 2, 7, 5))
  datetime.datetime(2016, 2, 7, 5, 0)
  >>> GetUnofficialBuildDate(datetime.datetime(2016, 2, 8, 5))
  datetime.datetime(2016, 2, 7, 5, 0)
  """

  if build_date.hour < 5:
    # The time is locked at 5:00 am in UTC to cause the build cache
    # invalidation to not happen exactly at midnight. Use the same calculation
    # as the day before.
    # See //base/build_time.cc.
    build_date = build_date - datetime.timedelta(days=1)
  build_date = datetime.datetime(build_date.year, build_date.month,
                                 build_date.day, 5, 0, 0)

  day = build_date.day
  month = build_date.month
  year = build_date.year
  first_sunday = GetFirstSundayOfMonth(year, month)
  # If our build is after the first Sunday, we've already refreshed our build
  # cache on a quiet day, so just use that day.
  # Otherwise, take the first Sunday of the previous month.
  if day >= first_sunday:
    day = first_sunday
  else:
    month -= 1
    if month == 0:
      month = 12
      year -= 1
    day = GetFirstSundayOfMonth(year, month)
  return datetime.datetime(
      year, month, day, build_date.hour, build_date.minute, build_date.second)


def main():
  if doctest.testmod()[0]:
    return 1
  argument_parser = argparse.ArgumentParser()
  argument_parser.add_argument(
      'build_type', help='The type of build', choices=('official', 'default'))
  args = argument_parser.parse_args()

  # The mtime of the revision in build/util/LASTCHANGE is stored in a file
  # next to it. Read it, to get a deterministic time close to "now".
  # That date is then modified as described at the top of the file so that
  # it changes less frequently than with every commit.
  # This intentionally always uses build/util/LASTCHANGE's commit time even if
  # use_dummy_lastchange is set.
  lastchange_file = os.path.join(THIS_DIR, 'util', 'LASTCHANGE.committime')
  last_commit_timestamp = int(open(lastchange_file).read())
  build_date = datetime.datetime.fromtimestamp(last_commit_timestamp,
                                               datetime.timezone.utc)

  # For official builds we want full fidelity time stamps because official
  # builds are typically added to symbol servers and Windows symbol servers
  # use the link timestamp as the prime differentiator, but for unofficial
  # builds we do lots of quantization to avoid churn.
  offset = 0
  if args.build_type == 'official':
    if os.name == 'nt':
      version_path = os.path.join(THIS_DIR, os.pardir, 'chrome', 'VERSION')
      with open(version_path) as f:
        patch_line = f.readlines()[3].strip()
        # Use the patch number as an offset to the build date so that multiple
        # versions with different patch numbers built from the same source code
        # will get different build_date values. This is critical for Windows
        # symbol servers, to avoid collisions.
        assert patch_line.startswith('PATCH=')
        offset = int(patch_line[6:])
  else:
    build_date = GetUnofficialBuildDate(build_date)
  print(offset + int(calendar.timegm(build_date.utctimetuple())))
  return 0


if __name__ == '__main__':
  sys.exit(main())
