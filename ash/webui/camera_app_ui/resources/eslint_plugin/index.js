// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: https://astexplorer.net/ is useful when developing custom rules.
// Select "JavaScript", "@typescript-eslint/parser", "Transform: ESLint v4"
// on the top options.
//
// Note that the ESLint we're using is v7, so there's some difference. Most
// notably, the node type names we're getting in the rules are same as the one
// in the AST (e.g. "TSTypeAnnotation") instead of the one got in the ESLint
// pass on astexplorer.net (e.g. "TypeAnnotation").

const parameterCommentFormatRule = {
  create: (context) => {
    const sourceCode = context.getSourceCode();
    return {
      /* eslint-disable-next-line @typescript-eslint/naming-convention */
      CallExpression(node) {
        for (const arg of node.arguments) {
          const comments = sourceCode.getComments(arg);
          for (const comment of comments.leading) {
            const {type, value} = comment;
            if (type !== 'Block') {
              continue;
            }
            if (!value.match(/ \w+= /)) {
              context.report({
                node: comment,
                message: 'Inline block comment for parameters' +
                    ' should be in the form of /* var= */',
              });
            }
          }
        }
      },
    };
  },
};

/* global module */
module.exports = {
  rules: {
    'parameter-comment-format': parameterCommentFormatRule,
  },
};
