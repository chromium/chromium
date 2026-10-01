# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for Chrome Android preferences.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

import re

PRESUBMIT_VERSION = '2.0.0'

CHROME_PREFERENCE_KEYS_PATH = (
  'chrome/browser/preferences/android/java/src/org/chromium/chrome/browser/'
  'preferences/ChromePreferenceKeys.java'
)

DECLARATION_PATTERN = re.compile(
  r'public\s+static\s+final\s+(?:String|KeyPrefix)\s+([A-Z0-9_]+)\s*='
)

# Generic tokens identifying event counters, numeric aggregations, past-tense
# action logging, and impression/visibility tracking.
BANNED_SHARED_PREFS_COUNTER_PATTERNS_RE = re.compile(
  r'(?:^|_)(?:'
  # Counters & Quantities
  r'COUNTS?|COUNTERS?|'
  r'NUM|NUMBER|'
  r'TIMES|'
  r'ATTEMPTS?|'
  # Past-tense Actions / Interactions
  r'CLICKS?|CLICKED|'
  r'TAPS?|'
  r'DISMISSALS?|DISMISSED|'
  # Impressions & Visibility
  r'SHOWN|IMPRESSIONS?|SEEN'
  r')(?:_|$)',
  re.IGNORECASE,
)


def CheckNoNewSharedPreferencesCounters(input_api, output_api):
  """Checks that no new counter/impression/promo preference keys are added."""
  if input_api.no_diffs:
    return []

  affected_file = None
  for f in input_api.AffectedFiles(include_deletes=False):
    if f.UnixLocalPath() == CHROME_PREFERENCE_KEYS_PATH:
      affected_file = f
      break

  if not affected_file:
    return []

  old_contents_text = (
    '\n'.join(affected_file.OldContents())
    if affected_file.OldContents()
    else ''
  )
  new_contents_text = '\n'.join(affected_file.NewContents())

  old_constants = set(DECLARATION_PATTERN.findall(old_contents_text))
  new_constants = set(DECLARATION_PATTERN.findall(new_contents_text))

  added_constants = new_constants - old_constants

  problematic_keys = []
  for const_name in sorted(added_constants):
    if BANNED_SHARED_PREFS_COUNTER_PATTERNS_RE.search(const_name):
      problematic_keys.append(const_name)

  if not problematic_keys:
    return []

  formatted_keys = '\n'.join(f'  - {key}' for key in problematic_keys)
  warning_message = (
    'SharedPreferences Counter / Impression Anti-Pattern Check Failed:\n'
    'Based on the name(s) of your new preferences, it looks like you\n'
    'may be adding a SharedPreference for counting some action/event\n'
    'with a preference key to ChromePreferenceKeys.java:\n'
    f'{formatted_keys}\n\n'
    'Using SharedPreferences for counting events (or most "counting")\n'
    'is generally an anti-pattern and is discouraged. It can almost always\n'
    'be done using the feature engagement tracker, which does not persist\n'
    'forever on a device, and is self-cleaning. Some examples might be\n'
    '"New" labels, promo impression caps, gating based on interactions, '
    'etc.\n\n'
    'Please re-assess whether or not you need to be using '
    'SharedPreferences\n'
    'for this feature and whether you can instead use the FET.\n\n'
    'For questions please ping mschillaci@.'
  )

  return [output_api.PresubmitPromptWarning(warning_message)]
