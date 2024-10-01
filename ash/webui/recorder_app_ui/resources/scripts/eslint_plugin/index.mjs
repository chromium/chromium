// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This custom ESLint plugin includes several custom rules.
 * Please see variable `rules` at the end of file for a list of rules, and the
 * jsdoc comment on the functions that implements each rule (the values of the
 * `rules` object) for explanation of each rule.
 */

// Note: https://astexplorer.net/ is useful when developing custom rules.
// Select "JavaScript", "@typescript-eslint/parser", "Transform: ESLint v8"
// on the top options.

// The @typescript-eslint/utils import needs to be full path since there
// doesn't seem to be a way to tell ESLint to find the import at that path. And
// it needs to be in one single string so TypeScript would recognize the type
// import and infer types correctly.

/* eslint-disable @stylistic/max-len */
import {
  ESLintUtils,
  TSESLint,
  TSESTree,
} from
  '../../../../../../third_party/node/node_modules/@typescript-eslint/utils/dist/index.js';
/* eslint-enable @stylistic/max-len */

// This file is written in JavaScript with types in jsdoc, since ESLint can't
// use .ts file directly, and we don't want a separate compile pass before lint
// can be used.

// ESLint jsdoc doesn't recognize "asserts condition".
/* eslint-disable jsdoc/valid-types */
/**
 * @param {boolean} condition The condition that should be true.
 * @param {string=} optMessage Message when it's not true.
 * @return {asserts condition} Asserts `condition` is true.
 */
/* eslint-enable jsdoc/valid-types */
function assert(condition, optMessage) {
  if (!condition) {
    let message = 'Assertion failed';
    if (optMessage !== undefined) {
      message = message + ': ' + optMessage;
    }
    throw new Error(message);
  }
}

/**
 * Checks if the inline comment before arguments in function call ends with
 * '='.
 *
 * See go/tsjs-style#comments-when-calling-a-function.
 *
 * Note that in the following example, '|' should be replaced by '/', but
 * JavaScript doesn't support nested comment.
 * Example:
 *   foo(/* bar *| 1) should be written as foo(/* bar= *| 1) instead.
 */
const parameterCommentFormatRule = ESLintUtils.RuleCreator.withoutDocs({
  create: (context) => {
    const sourceCode = context.sourceCode;
    return {
      /* eslint-disable-next-line @typescript-eslint/naming-convention */
      CallExpression(node) {
        for (const arg of node.arguments) {
          const comments = sourceCode.getCommentsBefore(arg);
          for (const comment of comments) {
            const {type, value} = comment;
            if (type !== 'Block') {
              continue;
            }
            if (!value.match(/ \w+= /) &&
                !value.match(/^\s*eslint-disable-next-line/)) {
              context.report({
                node: comment,
                messageId: 'inlineCommentError',
              });
            }
          }
        }
      },
    };
  },
  meta: {
    messages: {
      inlineCommentError: 'Inline block comment for parameters' +
        ' should be in the form of /* var= */',
    },
    type: 'suggestion',
    schema: [],
  },
  defaultOptions: [],
});

/**
 * Checks whether generic parameters can be moved onto new expression.
 *
 * Example:
 *   `let s: Set<number> = new Set();` can be simplified to
 *   `let s = new Set<number>()`.
 */
const genericParameterOnDeclarationType = ESLintUtils.RuleCreator.withoutDocs({
  create: (context) => {
    /**
     * @param {TSESTree.TSTypeAnnotation|undefined} typeAnnotation Type
     *     annotation.
     * @param {TSESTree.Expression|null} value The value expression.
     */
    function checkTypeParameterSameAsNewExpression(typeAnnotation, value) {
      if (value === null || value.type !== 'NewExpression' ||
          value.callee.type !== 'Identifier') {
        return;
      }
      const newTypeName = value.callee.name;

      if (typeAnnotation === undefined ||
          typeAnnotation.type !== 'TSTypeAnnotation' ||
          typeAnnotation.typeAnnotation.type !== 'TSTypeReference' ||
          typeAnnotation.typeAnnotation.typeName.type !== 'Identifier') {
        return;
      }
      const typeAnnotationTypeName =
        typeAnnotation.typeAnnotation.typeName.name;

      if (newTypeName === typeAnnotationTypeName) {
        if (typeAnnotation.typeAnnotation.typeArguments !== undefined) {
          context.report({
            node: typeAnnotation.typeAnnotation.typeArguments,
            messageId: 'genericTypeParametersToNew',
          });
        } else {
          context.report({
            node: typeAnnotation.typeAnnotation,
            messageId: 'redundantType',
          });
        }
      }
    }
    return {
      /* eslint-disable-next-line @typescript-eslint/naming-convention */
      PropertyDefinition({value, typeAnnotation}) {
        checkTypeParameterSameAsNewExpression(typeAnnotation, value);
      },
      /* eslint-disable-next-line @typescript-eslint/naming-convention */
      VariableDeclarator({id: {typeAnnotation}, init}) {
        checkTypeParameterSameAsNewExpression(typeAnnotation, init);
      },
    };
  },
  meta: {
    messages: {
      genericTypeParametersToNew:
        'Generic type parameters can be moved to the new expression.',
      redundantType: 'Redundant type annotation.',
    },
    type: 'suggestion',
    schema: [],
  },
  defaultOptions: [],
});

/* eslint-disable cra/todo-format */
const BAD_TODO_FORMAT_REGEX = new RegExp(
  'TODO' +
    ('(' +
     '\\(' +            // Old format TODO: Starts with 'TODO('
     '(?![a-z]+\\))' +  // And isn't followed by '<ldap>)'
     '|' +
     ': ' +                 // New format TODO: Starts with 'TODO:'
     '(?!(b\\/\\d+) - )' +  // And isn't followed by 'b/<num> - '
     ')'),
  'gd',
);
/* eslint-enable cra/todo-format */

/**
 * @param {TSESTree.Position} loc The source location of the string.
 * @param {string} str The string.
 * @param {number} idx The offset in the string.
 * @return {TSESTree.Position} The source location of the character at
 *     that offset of the string.
 */
function getLocationAtOffset(loc, str, idx) {
  const {line, column} = loc;
  const prefix = str.substring(0, idx);
  const newLines = Array.from(prefix.matchAll(/\n/dg));
  if (newLines.length === 0) {
    return {
      line,
      column: column + idx,
    };
  }
  const lastNewLine = newLines[newLines.length - 1];
  assert(lastNewLine.indices !== undefined);
  // [0] for the whole match, and [1] for the match range ending.
  const resultColumn = idx - lastNewLine.indices[0][1];
  const resultLine = line + newLines.length;
  return {line: resultLine, column: resultColumn};
}

/**
 * @template {string} T
 * @template {unknown[]} O
 * @param {Readonly<TSESLint.RuleContext<T, O>>} context The ESLint
 *     context.
 * @param {TSESTree.SourceLocation} loc The source location of the string.
 * @param {string} str The string to be matched against.
 * @param {RegExp} regex The regex to be matched.
 * @param {T} messageId The error message.
 */
function reportRegexMatches(context, loc, str, regex, messageId) {
  for (const match of str.matchAll(regex)) {
    assert(match.indices !== undefined, `Regex should have 'd' flag on`);
    const [startIdx, endIdx] = match.indices[0];
    const regexLoc = {
      start: getLocationAtOffset(loc.start, str, startIdx),
      end: getLocationAtOffset(loc.start, str, endIdx),
    };
    context.report({
      messageId,
      loc: regexLoc,
    });
  }
}

/**
 * Checks the TODO in comments match style in go/todo-style.
 *
 * Specifically, we currently allow the following:
 *  - Old style with ldap `TODO(ldap): xxx`.
 *  - New style with bug link `TODO: b/123 - xxx`.
 *
 * Non-trivial TODOs should be tracked in a bug and use the new style TODO.
 */
const todoFormatRule = ESLintUtils.RuleCreator.withoutDocs({
  create: (context) => {
    const {sourceCode} = context;
    return {
      /* eslint-disable-next-line @typescript-eslint/naming-convention */
      Program() {
        const comments = sourceCode.getAllComments();
        for (const comment of comments) {
          const commentValue = comment.value;
          reportRegexMatches(
            context,
            comment.loc,
            commentValue,
            BAD_TODO_FORMAT_REGEX,
            'invalidTodo',
          );
        }
      },
      // Also check TODO strings inside css`` and html`` tag.
      /* eslint-disable-next-line @typescript-eslint/naming-convention */
      TaggedTemplateExpression({tag, quasi}) {
        if (tag.type !== 'Identifier') {
          return;
        }
        if (tag.name !== 'css' && tag.name !== 'html') {
          return;
        }
        for (const el of quasi.quasis) {
          reportRegexMatches(
            context,
            el.loc,
            el.value.raw,
            BAD_TODO_FORMAT_REGEX,
            'invalidTodo',
          );
        }
      },
    };
  },
  meta: {
    messages: {
      invalidTodo:
        'Use either `TODO(ldap)` or `TODO: b/123 -`, see go/todo-style',
    },
    type: 'suggestion',
    schema: [],
  },
  defaultOptions: [],
});

// TODO(pihsun): Add a rule for checking string enum order that can be applied
// to only specific enums.
// TODO(pihsun): Add a rule for checking object literal {foo: foo} -> {foo}.

const rules = {
  'parameter-comment-format': parameterCommentFormatRule,
  'generic-parameter-on-declaration-type': genericParameterOnDeclarationType,
  'todo-format': todoFormatRule,
};

export default {rules};
