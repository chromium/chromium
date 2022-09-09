#!/bin/bash
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file formats the auto-generated parser.js so it passes presubmit checks.

if ! grep -q 'Copyright' parser.js; then
  sed -i '0,/\//{s|/|// Copyright 2021 The Chromium Authors. Al#\n/|}' parser.js
  sed -i '0,/#/{s|#|l rights reserved.\n// Use of this source code#|}' parser.js
  sed -i '0,/#/{s|#| is governed by a BSD-style license that can b#|}' parser.js
  sed -i '0,/#/{s|#|e\n// found in the LICENSE file.\n|}' parser.js
fi

sed -i 's/show_input_position == undefined/!show_input_position/g' parser.js
sed -i "s|[ \t\n]*// can't ever have more input lines than this![ \t\n]*| |g" \
  parser.js

sed -i "s/recovery approach availabl/recovery approach '+'availabl/g" parser.js
sed -i "s/the lexer is of/the '+'lexer is of/g" parser.js
sed -i "s/persuasion (options./persuasion '+'(options./g" parser.js
sed -i "s/non-existing condition/non-existing '+'condition/g" parser.js
sed -i "s/the application programmer/the '+'application programmer/g" parser.js

sed -i 's/preceeding/preceding/g' parser.js

sed -i 's/sharedState_yy/sharedStateYy/g' parser.js
sed -i 's/this_production/thisProduction/g' parser.js
sed -i 's/pretty_src/prettySrc/g' parser.js
sed -i 's/pos_str/posStr/g' parser.js
sed -i 's/lineno_msg/linenoMsg/g' parser.js
sed -i 's/rule_re/ruleRe/g' parser.js
sed -i 's/rule_ids/ruleIds/g' parser.js
sed -i 's/rule_regexes/ruleRegexes/g' parser.js
sed -i 's/rule_new_ids/ruleNewIds/g' parser.js
sed -i 's/slice_len/sliceLen/g' parser.js
sed -i 's/pre_lines/preLines/g' parser.js
sed -i 's/lineno_display_width/linenoDisplayWidth/g' parser.js
sed -i 's/ws_prefix/wsPrefix/g' parser.js
sed -i 's/nonempty_line_indexes/nonemptyLineIndexes/g' parser.js
sed -i 's/lno_pfx/lnoPfx/g' parser.js
sed -i 's/clip_start/clipStart/g' parser.js
sed -i 's/clip_end/clipEnd/g' parser.js
sed -i 's/intermediate_line/intermediateLine/g' parser.js
sed -i 's/yy_/yY/g' parser.js
sed -i 's/MINIMUM_VISIBLE_NONEMPTY_LINE_COUNT/MIN_VIS_LINE/g' parser.js

git cl format --js parser.js

node_dir='../../../../../../../../third_party/node'

${node_dir}/linux/node-linux-x64/bin/node \
  ${node_dir}/node_modules/eslint/bin/eslint \
  --resolve-plugins-relative-to ${node_dir}/node_modules \
  --ignore-pattern .eslintrc.js parser.js --fix

git cl format --js parser.js
