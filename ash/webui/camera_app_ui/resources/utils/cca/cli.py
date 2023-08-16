# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""The utilities for parsing command line arguments"""

from __future__ import annotations

import argparse
import inspect
from typing import (
    Any,
    Callable,
    Dict,
    List,
    NamedTuple,
    Optional,
    Tuple,
    Union,
)


class CLIError(Exception):
    """Failed when processing CLI commands."""


class _Option(NamedTuple):
    """Parameters to build a command line option with add_argument()."""

    args: Tuple[Any, ...]
    kwargs: Dict[str, Any]


class _Command:
    """Parameters to build a command with add_parser()."""
    def __init__(self, func: Callable):
        self._func = func
        self._name: Optional[str] = None
        self._parent: Optional[_Command] = None
        self._options: List[_Option] = []

        # This will be arguments passed to argparse.ArgumentParser for root
        # command, and add_parser for non-root commands.
        self._kwargs: Dict[str, Any] = {}
        self._children: List[_Command] = []

    def __call__(self, *args, **kwargs):
        """Call the wrapped function."""
        return self._func(*args, **kwargs)

    def _set_children(self, children: List[_Command]):
        self._children = children
        for child in children:
            if child._parent is not None:
                raise CLIError(f"{child._name} should have only one parent")
            child._parent = self

    def build_parsers(self, parser: argparse.ArgumentParser):
        """Builds parsers by traversing the command tree.

        Args:
            parser: The parser of the current command node.
            func: The handler function of the current command node.
        """
        for opt in reversed(self._options):
            parser.add_argument(*opt.args, **opt.kwargs)
        parser.set_defaults(_cmd=self, _parser=parser)

        if self._children:
            subparsers = parser.add_subparsers(title="commands")
            for child in self._children:
                if child._name is None:
                    raise CLIError(
                        f"Children {child._func} should be wrapped with"
                        " @command")
                subparser = subparsers.add_parser(child._name, **child._kwargs)
                child.build_parsers(subparser)

    def run(self, argv: Optional[List[str]] = None) -> Optional[int]:
        """Parses the arguments and runs the commands.

        The command must be an root command

        Args:
            argv: The command line arguments.

        Returns:
            An optional return code for sys.exit().
        """
        if self._name is not None:
            raise CLIError("run can only be called on root command")
        parser = argparse.ArgumentParser(**self._kwargs)
        self.build_parsers(parser)

        args = parser.parse_args(argv)

        if args._cmd._children:
            # Print help if it has deeper subcommands not specified by the
            # command line arguments yet.
            args._parser.print_help()
            return

        cmd: Optional[_Command] = args._cmd
        cmds: List[_Command] = []
        while cmd is not None:
            cmds.append(cmd)
            cmd = cmd._parent

        # Process the command handlers from root to leaf.
        for cmd in reversed(cmds):
            # Extract the function parameters from parsed arguments.
            params = inspect.signature(cmd._func).parameters
            unwrapped_args = {k: getattr(args, k) for k in params}

            # Invoke the handler and return early if there is an error
            # indicated by return code.
            ret = cmd(**unwrapped_args)

            if ret is not None and ret != 0:
                return ret


_MaybeCommand = Union[Callable, _Command]


def _ensure_command(cmd: _MaybeCommand) -> _Command:
    if isinstance(cmd, _Command):
        return cmd
    return _Command(cmd)


_Decorator = Callable[[_MaybeCommand], _Command]


def command(name: str,
            *,
            children: Optional[List[_Command]] = None,
            **kwargs) -> _Decorator:
    """Decorator to create a new command.

    Args:
        name: The command name.
        children: A list of subcommands of the command.
        **kwargs: The keyword arguments to be forwarded to add_parser().
    """
    if children is None:
        children = []

    def decorator(func: _MaybeCommand) -> _Command:
        func = _ensure_command(func)
        if func._name is not None:
            raise CLIError("@command should only be used once per function")
        func._name = name
        func._kwargs = kwargs
        func._set_children(children)

        return func

    return decorator


def option(*args, **kwargs) -> _Decorator:
    """Decorator to register an option for the current command.

    Args:
        *args: The arguments to be forwarded to add_argument().
        **kwargs: The keyword arguments to be forwarded to add_parser().
    """
    def decorator(func: _MaybeCommand) -> _Command:
        func = _ensure_command(func)
        func._options.append(_Option(args, kwargs))
        return func

    return decorator


def root(*, children: Optional[List[_Command]] = None, **kwargs) -> _Decorator:
    """Decorator to create a new root command.

    Args:
        children: A list of subcommands of the command.
        **kwargs: The keyword arguments to be forwarded to add_parser().
    """
    if children is None:
        children = []

    def decorator(func: _MaybeCommand):
        func = _ensure_command(func)
        func._kwargs = kwargs
        func._set_children(children)

        return func

    return decorator
