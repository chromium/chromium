# Lint as: python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Helper script to use GN's JSON interface to make changes.

AST implementation details:
  https://gn.googlesource.com/gn/+/refs/heads/main/src/gn/parse_tree.cc

To dump an AST:
  gn format --dump-tree=json BUILD.gn > foo.json
"""

from __future__ import annotations

import dataclasses
import functools
import json
import subprocess
from typing import Callable, Dict, List, Optional, Tuple, TypeVar

NODE_CHILD = 'child'
NODE_TYPE = 'type'
NODE_VALUE = 'value'

_T = TypeVar('_T')


def _create_location_node(begin_line=1):
    return {
        'begin_column': 1,
        'begin_line': begin_line,
        'end_column': 2,
        'end_line': begin_line,
    }


def _wrap(node: dict):
    kind = node[NODE_TYPE]
    if kind == 'LIST':
        return StringList(node)
    if kind == 'BLOCK':
        return BlockWrapper(node)
    return NodeWrapper(node)


def _unwrap(thing):
    if isinstance(thing, NodeWrapper):
        return thing.node
    return thing


def _find_node(root_node: dict, target_node: dict):
    def recurse(node: dict) -> Optional[Tuple[dict, int]]:
        children = node.get(NODE_CHILD)
        if children:
            for i, child in enumerate(children):
                if child is target_node:
                    return node, i
                ret = recurse(child)
                if ret is not None:
                    return ret
        return None

    ret = recurse(root_node)
    if ret is None:
        raise Exception(
            f'Node not found: {target_node}\nLooked in: {root_node}')
    return ret

@dataclasses.dataclass
class NodeWrapper:
    """Base class for all wrappers."""
    node: dict

    @property
    def node_type(self) -> str:
        return self.node[NODE_TYPE]

    @property
    def node_value(self) -> str:
        return self.node[NODE_VALUE]

    @property
    def node_children(self) -> List[dict]:
        return self.node[NODE_CHILD]

    @functools.cached_property
    def first_child(self):
        return _wrap(self.node_children[0])

    @functools.cached_property
    def second_child(self):
        return _wrap(self.node_children[1])

    def is_list(self):
        return self.node_type == 'LIST'

    def is_identifier(self):
        return self.node_type == 'IDENTIFIER'

    def visit_nodes(self, callback: Callable[[dict],
                                             Optional[_T]]) -> List[_T]:
        ret = []

        def recurse(root: dict):
            value = callback(root)
            if value is not None:
                ret.append(value)
                return
            children = root.get(NODE_CHILD)
            if children:
                for child in children:
                    recurse(child)

        recurse(self.node)
        return ret

    def set_location_recursive(self, line):
        def helper(n: dict):
            loc = n.get('location')
            if loc:
                loc['begin_line'] = line
                loc['end_line'] = line

        self.visit_nodes(helper)

    def add_child(self, node, *, before=None):
        node = _unwrap(node)
        if before is None:
            self.node_children.append(node)
        else:
            before = _unwrap(before)
            parent_node, child_idx = _find_node(self.node, before)
            parent_node[NODE_CHILD].insert(child_idx, node)

            # Prevent blank lines between |before| and |node|.
            target_line = before['location']['begin_line']
            _wrap(node).set_location_recursive(target_line)

    def remove_child(self, node):
        node = _unwrap(node)
        parent_node, child_idx = _find_node(self.node, node)
        parent_node[NODE_CHILD].pop(child_idx)


@dataclasses.dataclass
class BlockWrapper(NodeWrapper):
    """Wraps a BLOCK node."""
    def __post_init__(self):
        assert self.node_type == 'BLOCK'

    def find_assignments(self, var_name=None):
        def match_fn(node: dict):
            assignment = AssignmentWrapper.from_node(node)
            if not assignment:
                return None
            if var_name is None or var_name == assignment.variable_name:
                return assignment
            return None

        return self.visit_nodes(match_fn)


@dataclasses.dataclass
class AssignmentWrapper(NodeWrapper):
    """Wraps a =, +=, or -= BINARY node where the LHS is an identifier."""
    def __post_init__(self):
        assert self.node_type == 'BINARY'

    @property
    def variable_name(self):
        return self.first_child.node_value

    @property
    def value(self):
        return self.second_child

    @property
    def list_value(self):
        ret = self.second_child
        assert isinstance(ret, StringList), 'Found: ' + ret.node_type
        return ret

    @property
    def operation(self):
        """The assignment operation. Either "=" or "+="."""
        return self.node_value

    @property
    def is_append(self):
        return self.operation == '+='

    def value_as_string_list(self):
        return StringList(self.value.node)

    @staticmethod
    def from_node(node: dict) -> Optional[AssignmentWrapper]:
        if node.get(NODE_TYPE) != 'BINARY':
            return None
        children = node[NODE_CHILD]
        assert len(children) == 2, (
            'Binary nodes should have two child nodes, but the node is: '
            f'{node}')
        left_child, right_child = children
        if left_child.get(NODE_TYPE) != 'IDENTIFIER':
            return None
        if node.get(NODE_VALUE) not in ('=', '+=', '-='):
            return None
        return AssignmentWrapper(node)

    @staticmethod
    def create(variable_name, value, operation='='):
        value_node = _unwrap(value)
        id_node = {
            'location': _create_location_node(),
            'type': 'IDENTIFIER',
            'value': variable_name,
        }
        return AssignmentWrapper({
            'location': _create_location_node(),
            'child': [id_node, value_node],
            'type': 'BINARY',
            'value': operation,
        })

    @staticmethod
    def create_list(variable_name, operation='='):
        return AssignmentWrapper.create(variable_name,
                                        StringList.create(),
                                        operation=operation)


@dataclasses.dataclass
class StringList(NodeWrapper):
    """Wraps a list node that contains only string literals."""
    def __post_init__(self):
        assert self.is_list()

        self.literals: List[str] = [
            x[NODE_VALUE].strip('"') for x in self.node_children
            if x[NODE_TYPE] == 'LITERAL'
        ]

    def add_literal(self, value: str):
        # For lists of deps, gn format will sort entries, but it will not
        # move entries past comment boundaries. Insert at the front by default
        # so that if sorting moves the value, and there is a comment boundary,
        # it will end up before the comment instead of immediately after the
        # comment (which likely does not apply to it).
        self.literals.insert(0, value)
        self.node_children.insert(
            0, {
                'location': _create_location_node(),
                'type': 'LITERAL',
                'value': f'"{value}"',
            })

    def remove_literal(self, value: str):
        self.literals.remove(value)
        quoted = f'"{value}"'
        children = self.node_children
        for i, node in enumerate(children):
            if node[NODE_VALUE] == quoted:
                children.pop(i)
                break
        else:
            raise ValueError(f'Did not find child with value {quoted}')

    @staticmethod
    def create() -> StringList:
        return StringList({
            'location': _create_location_node(),
            'begin_token': '[',
            'child': [],
            'end': {
                'location': _create_location_node(),
                'type': 'END',
                'value': ']'
            },
            'type': 'LIST',
        })


class Target(NodeWrapper):
    """Wraps a target node.

    A target node is any function besides "template" with exactly two children:
      * Child 1: LIST with single string literal child
      * Child 2: BLOCK

    This does not actually find all targets. E.g. ignores those that use an
    expression for a name, or that use "target(type, name)".
    """
    def __init__(self, function_node: dict, name_node: dict):
        super().__init__(function_node)
        self.name_node = name_node

    @property
    def name(self) -> str:
        return self.name_node[NODE_VALUE].strip('"')

    # E.g. "android_library"
    @property
    def type(self) -> str:
        return self.node[NODE_VALUE]

    @property
    def block(self) -> BlockWrapper:
        block = self.second_child
        assert isinstance(block, BlockWrapper)
        return block

    def set_name(self, value):
        self.name_node[NODE_VALUE] = f'"{value}"'

    @staticmethod
    def from_node(node: dict) -> Optional[Target]:
        """Returns a Target if |node| is a target, None otherwise."""
        if node.get(NODE_TYPE) != 'FUNCTION':
            return None
        if node.get(NODE_VALUE) == 'template':
            return None
        children = node.get(NODE_CHILD)
        if not children or len(children) != 2:
            return None
        func_params_node, block_node = children
        if block_node.get(NODE_TYPE) != 'BLOCK':
            return None
        if func_params_node.get(NODE_TYPE) != 'LIST':
            return None
        param_nodes = func_params_node.get(NODE_CHILD)
        if param_nodes is None or len(param_nodes) != 1:
            return None
        name_node = param_nodes[0]
        if name_node.get(NODE_TYPE) != 'LITERAL':
            return None
        return Target(function_node=node, name_node=name_node)


class BuildFile:
    """Represents the contents of a BUILD.gn file."""
    def __init__(self, path: str, root_node: dict):
        self.block = BlockWrapper(root_node)
        self.path = path
        self._original_content = json.dumps(root_node)

    def write_changes(self) -> bool:
        """Returns whether there were any changes."""
        new_content = json.dumps(self.block.node)
        if new_content == self._original_content:
            return False
        output = subprocess.check_output(
            ['gn', 'format', '--read-tree=json', self.path],
            text=True,
            input=new_content)
        if 'Wrote rebuilt from json to' not in output:
            raise Exception('JSON was invalid')
        return True

    @functools.cached_property
    def targets(self) -> List[Target]:
        return self.block.visit_nodes(Target.from_node)

    @functools.cached_property
    def targets_by_name(self) -> Dict[str, Target]:
        return {t.name: t for t in self.targets}

    @staticmethod
    def from_file(path):
        output = subprocess.check_output(
            ['gn', 'format', '--dump-tree=json', path], text=True)
        return BuildFile(path, json.loads(output))
