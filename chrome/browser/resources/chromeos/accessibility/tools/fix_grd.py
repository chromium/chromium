#!/usr/bin/env python

# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''Enforces message text style.

Expects one argument to be the path of the .grd(p).

Sample usage:

./fix_grd.py some_messages.grd
git diff
# Audit changes (e.g. formatting, quotes, etc).
'''

import path_helpers
import optparse
import os
import re
import sys
import xml.etree.ElementTree as ET

def Die(message):
  '''Prints an error message and exit the program.'''
  sys.stderr.write(message + '\n')
  sys.exit(1)

def Process(xml_file):
  xml = ET.parse(xml_file)
  root = xml.getroot()
  messages = root.findall('message')
  modified = False
  removed_so_far = set()
  for message in messages:
    modified |= MaybeRemoveTrailingPeriods(message)
    modified |= MaybeRemoveUnusedMessage(root, message, removed_so_far)

  if modified:
    xml.write(xml_file, encoding='UTF-8')

def MaybeRemoveTrailingPeriods(message):
  modified = False
  # Re-write messages containing a period at the end (excluding whitespace)
  # and no other periods. This excludes phrases that have multiple sentences
  # which should retain periods.
  text = message.text.rstrip()
  if text.endswith('.') and text.find('.') == text.rfind('.'):
    modified = True
    message.text = message.text.replace('.', '')
  return modified

def MaybeRemoveUnusedMessage(root, message, removed_so_far):
  found = False

  # Always strip IDS_ and lowercase the message id.
  base_message_id = re.sub('^ids_', '', message.get('name').lower())

  # Get the unprefixed message id. This is used by various extensions like
  # ChromeVox and STS.
  message_id = re.sub(
    '^(chromevox_|select_to_speak_|switch_access_|enhanced_network_tts_)',
    '', base_message_id)

  # This message is needed by the extension system.
  if message_id == 'locale':
    return False

  # Explicitly skip these messages in ChromeVox since they get programmatically
  # constructed. If the non _brl counterpart was removed though, also remove it.
  if message_id.endswith('_brl'):
    if message_id[:-4] in removed_so_far:
      sys.stdout.write('Removing ' + message_id + '\n')
      root.remove(message)
      return True
    else:
      return False

  # Explicitly skip messages referencing ARIA; these strings should be used by
  # ChromeVox.
  if message.get('desc').find('ARIA') != -1:
    return False

  # Explicitly skip messages starting with tag_.
  if message_id.startswith('tag_'):
    return False

  for dir_name, subdir_list, file_list in os.walk(
      path_helpers.AccessibilityPath()):
    for fname in file_list:
      if not fname.endswith('.js') and not fname.endswith('.html'):
        continue
      with open(os.path.join(dir_name, fname), 'r') as f:
        for line in f:
          index = line.find(base_message_id)
          if index == -1:
            index = line.find(message_id)

          # Eliminate partial matches (e.g. for bar, foo_bar).
          if index > 0 and line[index - 1] == '_':
            continue

          if index != -1:
            found = True
            break

  if not found:
    sys.stdout.write('Removing ' + message_id + '\n')
    removed_so_far.add(message_id)
    root.remove(message)

  return not found

if __name__ == '__main__':
  options, args = optparse.OptionParser().parse_args()
  if len(args) != 1:
    Die('Expected one .grd file')
  Process(args[0])
