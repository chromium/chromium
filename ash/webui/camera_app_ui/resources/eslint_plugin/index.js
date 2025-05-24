// Copyright 2022 The Chromium Authors
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
    const {sourceCode} = context;
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

const genericParameterOnDeclarationType = {
  create: (context) => {
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
        if (typeAnnotation.typeAnnotation.typeParameters !== undefined) {
          context.report({
            node: typeAnnotation.typeAnnotation.typeParameters,
            message:
                'Generic type parameters can be moved to the new expression.',
          });
        } else {
          context.report({
            node: typeAnnotation.typeAnnotation,
            message: 'Redundant type annotation.',
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
};

const todoFormatRule = {
  create: (context) => {
    function verifyTodo(comment) {
      return !/TODO(?!\((b\/\d+|crbug\.com\/\d+|[a-z]+)\))/g.test(comment);
    }

    const {sourceCode} = context;
    return {
      /* eslint-disable-next-line @typescript-eslint/naming-convention */
      Program: function() {
        const comments = sourceCode.getAllComments();
        for (const comment of comments) {
          const commentValue = comment.value;
          if (!verifyTodo(commentValue)) {
            context.report({
              node: comment,
              message: `Use: TODO(ldap) / TODO(b/123456789) /` +
                  ` TODO(crbug.com/123456789)`,
            });
          }
        }
      },
    };
  },
};

const stringEnumOrder = {
  create: (context) => {
    return {
      /* eslint-disable-next-line @typescript-eslint/naming-convention */
      TSEnumDeclaration(node) {
        if (!node.members.every(
                (member) => typeof member.initializer?.value === 'string')) {
          // Skip if it isn't a string enum.
          return;
        }
        const sortedNames =
            node.members.map((member) => member.id.name)
                .sort(
                    (a, b) => a.toLowerCase().localeCompare(b.toLowerCase()),
                );
        for (let i = 0; i < node.members.length; i++) {
          const correctName = sortedNames[i];
          const member = node.members[i];
          if (member.id.name !== correctName) {
            context.report({
              node: member,
              message: `"${member.id.name}" should be after "${correctName}".`,
            });
            break;
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
    'generic-parameter-on-declaration-type': genericParameterOnDeclarationType,
    'todo-format': todoFormatRule,
    'string-enum-order': stringEnumOrder,
  },
};
