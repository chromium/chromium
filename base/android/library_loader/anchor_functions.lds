# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Define symbols that point to the start and end of the .text section.
PROVIDE_HIDDEN(linker_script_start_of_text = ADDR(.text));

# The `malloc_hook` section comes from function attributes set in
# third_party/abseil-cpp. See http://crbug.com/352317042.
PROVIDE_HIDDEN(linker_script_end_of_text =
    ADDR(.text) + SIZEOF(.text) + SIZEOF(malloc_hook));
