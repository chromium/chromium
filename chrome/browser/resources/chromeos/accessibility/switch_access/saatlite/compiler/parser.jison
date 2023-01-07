// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* This file compiles SAATLite tests into JS tests. */

// Preamble:
%{
  let page, point, objectToMatch;

  let indent = '';
  const increaseIndent = () => indent = indent + '  ';
  const decreaseIndent = () => indent = indent.substring(2);

  let buffer = '';
  const addToBuffer = (text) => buffer += indent + text + '\n';
  const flushBuffer = () => {
    const result = buffer;
    buffer = '';
    return result;
  }

  function addAllStatements(statements) {
    if (statements.find(statement => !statement.output)) {
      throw new Error('Compiler error: statement not converted to Javascript');
    }
    statements.forEach(statement => addToBuffer(statement.output));
    return flushBuffer();
  }

  // Initialize test before anything else.
  const initTest = (page = `''`) => {
    addToBuffer(`TEST_F('SwitchAccessSAATLiteTest', 'Demo', function() {`);
    increaseIndent();
    addToBuffer(`this.runWithLoadedTree(${page.output}, async (rootWebArea) => {`);
    increaseIndent();
    addToBuffer('TestUtility.startFocusInside(rootWebArea);');
    return flushBuffer();
  }

  const finishTest = (opt_url) => {
    decreaseIndent();
    if (opt_url) {
      addToBuffer(`}, {url: ${opt_url.output}});`);
    } else {
      addToBuffer(`});`);
    }
    decreaseIndent();
    addToBuffer(`});`);

    return flushBuffer();
  }
%}

%start test

%%

test
  : load_expression statements EOF {
      $$ = {};
      if ($1.page.type === 'HTML') {
        $$.output = initTest($1.page);
      } else {
        $$.output = initTest();
      }

      $$.output += addAllStatements($2);

      if ($1.page.type === 'ChromeURL') {
        $$.output += finishTest($1.page);
      } else {
        $$.output += finishTest();
      }

      $2.unshift($1);
      $$.ast = $2;
      return $$;
    }
  | statements EOF {
      $$ = {ast: $1};
      $$.output = initTest();
      $$.output += addAllStatements($1);
      $$.output += finishTest();

      return $$;
    }
  ;

load_expression
  : LOAD_PAGE page_expression {
      $$ = {command: 'Load', page: $2};
    }
  | LOAD_PAGE EOL page_expression {
      $$ = {command: 'Load', page: $3};
    }
  ;

statements
  // Expect a new line between statements.
  : statements EOL statement {
      $$ = $1;
      $$.push($3);
    }
  // Allow arbitrary empty lines.
  | statements EOL {
      $$ = $1;
    }
  | statement {
      $$ = [$1];
    }
  | /* Empty */ {
      $$ = [];
    }
  ;

statement
  : NEXT {
      $$ = {command: 'Next'};
      $$.output = 'TestUtility.pressNextSwitch();';
    }
  | PREVIOUS {
      $$ = {command: 'Previous'};
      $$.output = 'TestUtility.pressPreviousSwitch();';
    }
  | SELECT point_expression {
      $$ = {command: 'Select'};
      // A point_expression is only expected when point_scan is enabled.
      if ($2) {
        $$.point = $2;
        point = $$.point.output;
        $$.output = `TestUtility.simulatePointScanSelect(${point});`;
      } else {
        $$.output = 'TestUtility.pressSelectSwitch();';
      }
    }
  | EXPECT expectation_expression {
      $$ = {command: 'Expect', expectation: $2};
      $$.output = $2.output;
    }
  ;

page_expression
  : html_page {
      $$ = {type: 'HTML', value: $1};
      page = $$.value;
      $$.output = `'` + page + `'`;
    }
  | CHROME_URL {
      $$ = {type: 'ChromeURL', value: $1};
      page = $$.value;
      $$.output = `'` + page + `'`;
    }
  ;

html_page
  : html_page HTML_SNIPPET {
      $$ = $1 + '\n' + $2;
    }
  | HTML_SNIPPET {
      $$ = $1;
    }
  ;

point_expression
  : LEFT_PARENS NUMBER COMMA NUMBER RIGHT_PARENS {
      $$ = {x: $2, y: $4};
      point = $$;
      $$.output = `{x: ${point.x}, y: ${point.y}}`;
    }
  | /* Empty */ {
      $$ = null;
    }
  ;

expectation_expression
  : FOCUS ON focusable_expression {
      $$ = {type: 'Focus', matches: $3};
      objectToMatch = $$.matches.output;
      $$.output = `await TestUtility.expectFocusOn(${objectToMatch});`;
    }
  ;

focusable_expression
  : ROLE string_literal {
      $$ = {role: $1, name: $2};
      objectToMatch = $$;
      $$.output = `{role: '${objectToMatch.role}', name: '${objectToMatch.name}'}`;
    }
  | string_literal ROLE {
      $$ = {role: $2, name: $1};
      objectToMatch = $$;
      $$.output = `{role: '${objectToMatch.role}', name: '${objectToMatch.name}'}`;
    }
  | ROLE {
      $$ = {role: $1};
      objectToMatch = $$;
      $$.output = `{role: '${objectToMatch.role}'}`;
    }
  | string_literal {
      $$ = {name: $1};
      objectToMatch = $$;
      $$.output = `{name: '${objectToMatch.name}'}`;
    }
  ;

string_literal
  : STRING_LITERAL {
      // Remove the quotes from the string.
      $$ = $1.substring(1, $1.length - 1);
    }
  ;
