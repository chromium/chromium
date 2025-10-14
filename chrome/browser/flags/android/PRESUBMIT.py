# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for Chrome Android feature flag code."""

def CheckChromeFeatureIsSorted(input_api, output_api, file_path, name, start_pattern, end_pattern):
  """
  Checks that the section of code within `start_pattern` and `end_pattern`
  inside the `file_path` file is sorted alphabetically.
  """
  # Find the file in the list of changed files.
  affected_file = None
  for f in input_api.AffectedFiles():
    if f.LocalPath() == file_path:
      affected_file = f
      break

  # If the file was not modified in this change, return.
  if not affected_file:
    return []

  contents = affected_file.NewContents()
  in_array = False
  feature_lines = []

  # Extract the lines that are inside the array definition.
  for line in contents:
    stripped_line = line.strip()
    if not in_array and start_pattern in line:
      in_array = True

    if in_array:
      if stripped_line == end_pattern:
        break  # We've reached the end of the array.
      # Ignore comments and empty lines within the array.
      if stripped_line and not stripped_line.startswith('//'):
        feature_lines.append(stripped_line)

  # If the array wasn't found or is empty, return.
  if not feature_lines:
    return []

  # Create a sorted version of the list for comparison.
  # The sort is not case-sensitive to be user-friendly.
  sorted_feature_names = sorted(feature_lines)

  # If the list is already sorted, the check passes.
  if feature_lines == sorted_feature_names:
    return []

  # If the list is not sorted, find the first discrepancy to create a
  # helpful and actionable error message for the developer.
  error_detail = ''
  for i, original in enumerate(feature_lines):
    if original != sorted_feature_names[i]:
      error_detail = (
          "The list is not sorted alphabetically.\n"
          f"  - The first out-of-order item is: '{original}'\n"
          f"  - The expected item was:          '{sorted_feature_names[i]}'"
      )
      break

  error_message = (
      f"The `{name}` values in {file_path} must be sorted "
      f"alphabetically.\n\n{error_detail}\n\nPlease sort the list to "
      "fix this error."
  )

  # We want this to be an error because it is an easy fix, and this list has
  # historically become out-of-order, which makes automated scripting harder.
  return [output_api.PresubmitError(error_message)]


def CheckChangeOnCommit(input_api, output_api):
  results = []

  # Check that the array of exported features is in order.
  results.extend(CheckChromeFeatureIsSorted(
    input_api,
    output_api,
    'chrome/browser/flags/android/chrome_feature_list.cc',
    'kFeaturesExposedToJava',
    '// FEATURE_EXPORT_LIST_START',
    '// FEATURE_EXPORT_LIST_END')
  )

  # Check that all feature definitions are in order.
  results.extend(CheckChromeFeatureIsSorted(input_api, output_api,
    'chrome/browser/flags/android/chrome_feature_list.cc',
    'BASE_FEATURE',
    '// BASE_FEATURE_START',
    '// BASE_FEATURE_END'))

  # Check that all feature declarations are in order.
  results.extend(CheckChromeFeatureIsSorted(input_api, output_api,
    'chrome/browser/flags/android/chrome_feature_list.h',
    'BASE_DECLARE_FEATURE',
    '// BASE_DECLARE_FEATURE_START',
    '// BASE_DECLARE_FEATURE_END'))

  return results
