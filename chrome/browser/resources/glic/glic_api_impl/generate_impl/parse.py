# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import codecs
import os
import re
import sys


def GetDirAbove(dirname: str):
    path = os.path.abspath(__file__)
    while True:
        path, tail = os.path.split(path)
        if not tail:
            return None
        if tail == dirname:
            return path


SOURCE_DIR = GetDirAbove('chrome')

sys.path.append(
    os.path.abspath(os.path.join(SOURCE_DIR, 'mojo/public/tools/mojom')), )

from mojom.parse import parser
from mojom.parse import ast
from mojom.generate import generator

GENERATE_GLIC_API_RE = re.compile(r'.*@generate glic_api')
TYPE_OVERRIDE_RE = re.compile(r'.*@glic_type\s+(\S+)')
GLIC_OPTIONAL_RE = re.compile(r'.*@glic_optional')
GLIC_IGNORE_RE = re.compile(r'.*@glic_ignore')
STRINGLIKE_ID_RE = re.compile(r'(tab|window)_id$')

IGNORE_LINE_RES = [
    re.compile(r'.*([L]INT\.IfChange|[L]INT\.ThenChange)'),
    re.compile(r'\s*//\s*//(components|chrome|tools|ui)/.*'),
    re.compile(r'\s*// Next version:'),
    re.compile(r'\s*///'),
    GENERATE_GLIC_API_RE,
    TYPE_OVERRIDE_RE,
    GLIC_OPTIONAL_RE,
    GLIC_IGNORE_RE,
]


def ParseAst(mojom_abspath):
    with codecs.open(mojom_abspath, encoding='utf-8') as f:
        tree = parser.Parse(f.read(), mojom_abspath, with_comments=True)
        tree.filename = mojom_abspath
        return tree


class MojomParser:

    def __init__(self):
        mojom_files = [
            'chrome/browser/glic/host/glic.mojom',
            'chrome/common/actor_webui.mojom',
            'chrome/common/glic_enums.mojom',
            'third_party/blink/public/mojom/content_extraction' +
            '/ai_page_content_metadata.mojom',
        ]
        self.mojom_trees = [
            ParseAst(os.path.join(SOURCE_DIR, f)) for f in mojom_files
        ]

    def GetAnnotatedEnums(self):
        # Returns a list of (enum_node, source_name)
        annotated_enums = []
        for tree in self.mojom_trees:
            if 'actor_webui.mojom' in tree.filename:
                source = 'actor'
            elif 'glic_enums.mojom' in tree.filename:
                source = 'glic_enums'
            else:
                source = 'glic'
            for v in tree.definition_list:
                if not isinstance(v, ast.Enum):
                    continue
                if v.comments_before and any(
                    (GENERATE_GLIC_API_RE.search(comment.value)
                     for comment in v.comments_before)):
                    annotated_enums.append((v, source))
        return annotated_enums
