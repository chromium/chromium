# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for Chrome Android feature flag code."""


def CheckChromeFeatureListIsSorted(input_api, output_api):
  """
  Checks that the `kFeaturesExposedToJava` array in
  chrome/browser/flags/android/chrome_feature_list.cc is sorted alphabetically.
  """
  # The specific file to check.
  file_path = 'chrome/browser/flags/android/chrome_feature_list.cc'

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

  # Define patterns to find the start and end of the array.
  start_pattern = 'const base::Feature* const kFeaturesExposedToJava[] = {'
  end_pattern = '};'

  # Extract the lines that are inside the array definition.
  for line in contents:
    stripped_line = line.strip()
    if start_pattern in line:
      in_array = True
      continue

    if in_array:
      if stripped_line == end_pattern:
        break  # We've reached the end of the array.
      # Ignore comments and empty lines within the array.
      if stripped_line and not stripped_line.startswith('//'):
        feature_lines.append(stripped_line)

  # If the array wasn't found or is empty, return.
  if not feature_lines:
    return []

  # From each line, extract the feature name, including namespace.
  # For example, '&features::kMyFeature,' becomes 'features::kMyFeature'.
  feature_names = []
  for line in feature_lines:
    name = line.strip()
    if name.startswith('&'):
      name = name[1:]
    if name.endswith(','):
      name = name[:-1]
    feature_names.append(name.strip())

  # Create a sorted version of the list for comparison.
  # The sort is not case-sensitive to be user-friendly.
  sorted_feature_names = sorted(feature_names, key=str.lower)

  # If the list is already sorted, the check passes.
  if feature_names == sorted_feature_names:
    return []

  # If the list is not sorted, find the first discrepancy to create a
  # helpful and actionable error message for the developer.
  error_detail = ''
  for i, original in enumerate(feature_names):
    if original != sorted_feature_names[i]:
      error_detail = (
          "The list is not sorted alphabetically.\n"
          f"  - The first out-of-order item is: '{original}'\n"
          f"  - The expected item was:          '{sorted_feature_names[i]}'"
      )
      break

  error_message = (
      f"The `kFeaturesExposedToJava` array in {file_path} must be sorted "
      f"alphabetically.\n\n{error_detail}\n\nPlease sort the list to "
      "fix this error."
  )

  # We want this to be an error because it is an easy fix, and this list has
  # historically become out-of-order, which makes automated scripting harder.
  return [output_api.PresubmitError(error_message)]


def CheckChangeOnCommit(input_api, output_api):
  return CheckChromeFeatureListIsSorted(input_api, output_api)
