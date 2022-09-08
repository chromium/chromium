# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Module containing base test results classes."""

# The test passed.
PASS = 'SUCCESS'

# The test was intentionally skipped.
SKIP = 'SKIPPED'

# The test failed.
FAIL = 'FAILURE'

# The test caused the containing process to crash.
CRASH = 'CRASH'

# The test timed out.
TIMEOUT = 'TIMEOUT'

# The test ran, but we couldn't determine what happened.
UNKNOWN = 'UNKNOWN'

# The test did not run.
NOTRUN = 'NOTRUN'
