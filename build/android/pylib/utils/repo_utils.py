# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from devil.utils import cmd_helper


def GetGitHeadSHA1(in_directory):
  """Returns the git hash tag for the given directory.

  Args:
    in_directory: The directory where git is to be run.
  """
  command_line = ['git', 'log', '-1', '--pretty=format:%H']
  output = cmd_helper.GetCmdOutput(command_line, cwd=in_directory)
  return output[0:40]


def GetGitOriginMasterHeadSHA1(in_directory):
  command_line = ['git', 'rev-parse', 'origin/master']
  output = cmd_helper.GetCmdOutput(command_line, cwd=in_directory)
  return output.strip()


def GetGitOriginMainHeadSHA1(in_directory):
  command_line = ['git', 'rev-parse', 'origin/main']
  output = cmd_helper.GetCmdOutput(command_line, cwd=in_directory)
  return output.strip()
