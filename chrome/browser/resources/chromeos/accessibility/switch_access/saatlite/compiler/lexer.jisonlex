// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

%options case-insensitive

%%
[<][^\n]+                       return 'HTML_SNIPPET';
chrome[:][/][/][^\n]*           return 'CHROME_URL';
\"[^"]*\"                       return 'STRING_LITERAL';

\n                              return 'EOL';
[0-9]+                          return 'NUMBER';
[(]                             return 'LEFT_PARENS';
[)]                             return 'RIGHT_PARENS';
[,]                             return 'COMMA';

expect                          return 'EXPECT';
focus                           return 'FOCUS';
load[ ]page                     return 'LOAD_PAGE';
next                            return 'NEXT';
on                              return 'ON';
previous                        return 'PREVIOUS';
select                          return 'SELECT';

button                          return 'ROLE';
slider                          return 'ROLE';
spinButton                      return 'ROLE';
textField                       return 'ROLE';
textFieldWithComboBox           return 'ROLE';

<<EOF>>                         return 'EOF';
\s                              /* Ignore whitespace */;
