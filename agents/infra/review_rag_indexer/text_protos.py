# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utility for parsing a subset of text proto format.

This is used instead of the standard `protobuf` module because we do not have
a good way to get the generated _pb2.py files for the dir_metadata.proto file.

A parser that does not require those such as `lark` is an option, but `lark` is
not currently available in CIPD.

Since the scope of textpb that we need to support for DIR_METADATA files is
limited, just maintain what we need.
"""

from collections.abc import Generator
from enum import StrEnum
import re
from typing import Any


class TokenKind(StrEnum):
    STRING = 'STRING'
    NUMBER = 'NUMBER'
    ID = 'ID'
    COLON = 'COLON'
    LBRACE = 'LBRACE'
    RBRACE = 'RBRACE'
    LBRACKET = 'LBRACKET'
    RBRACKET = 'RBRACKET'
    COMMA = 'COMMA'
    SKIP = 'SKIP'
    MISMATCH = 'MISMATCH'


def tokenize(text: str) -> Generator[tuple[TokenKind, str], None, None]:
    """Tokenizes text proto content."""
    text = re.sub(r'#.*', '', text)
    token_specification = [
        (TokenKind.STRING, r'"[^"\\]*(?:\\.[^"\\]*)*"'),
        (TokenKind.NUMBER, r'\d+'),
        (TokenKind.ID, r'[a-zA-Z_][a-zA-Z0-9_]*'),
        (TokenKind.COLON, r':'),
        (TokenKind.LBRACE, r'\{'),
        (TokenKind.RBRACE, r'\}'),
        (TokenKind.LBRACKET, r'\['),
        (TokenKind.RBRACKET, r'\]'),
        (TokenKind.COMMA, r','),
        (TokenKind.SKIP, r'[ \t\n]+'),
        (TokenKind.MISMATCH, r'.'),
    ]
    tok_regex = '|'.join('(?P<%s>%s)' % pair for pair in token_specification)
    for mo in re.finditer(tok_regex, text):
        kind = TokenKind(mo.lastgroup)
        value = mo.group()
        if kind == TokenKind.SKIP:
            continue
        elif kind == TokenKind.MISMATCH:
            raise RuntimeError(f'{value!r} unexpected')
        yield kind, value


class TextProtoParser:
    """A simple parser for a subset of text proto format."""

    def __init__(self, text: str):
        """Initializes the parser with the text to parse.

        Args:
            text: The text proto content to parse.
        """
        self.tokens = list(tokenize(text))
        self.pos = 0

    def peek(self) -> tuple[TokenKind, str] | None:
        """Peeks at the next token without consuming it.

        Returns:
            The next token as a tuple of (kind, value), or None if there are
            no more tokens.
        """
        if self.pos < len(self.tokens):
            return self.tokens[self.pos]
        return None

    def consume(
            self,
            expected_kind: TokenKind | None = None) -> tuple[TokenKind, str]:
        """Consumes and returns the next token.

        Args:
            expected_kind: If specified, raises SyntaxError if the next
                token's kind does not match this value.

        Returns:
            The consumed token as a tuple of (kind, value).

        Raises:
            SyntaxError: If there are no more tokens (unexpected EOF), or if
                expected_kind is specified and the next token does not match
                it.
        """
        tok = self.peek()
        if not tok:
            raise SyntaxError('Unexpected EOF')
        if expected_kind and tok[0] != expected_kind:
            raise SyntaxError(
                f"Expected {expected_kind}, got {tok[0]} ({tok[1]})")
        self.pos += 1
        return tok

    def parse(self) -> dict[str, Any]:
        """Parses the text proto content.

        Returns:
            A dictionary representing the parsed text proto. Repeated fields
            are represented as lists.
        """
        result = {}
        while self.peek():
            key, val = self.parse_field()
            if key in result:
                if isinstance(result[key], list):
                    result[key].append(val)
                else:
                    result[key] = [result[key], val]
            else:
                result[key] = val
        return result

    def parse_field(self) -> tuple[str, Any]:
        """Parses a single field (key-value pair).

        Returns:
            A tuple of (key, value) representing the parsed field.

        Raises:
            SyntaxError: If the field is malformed or unexpected EOF is
                encountered.
        """
        id_tok = self.consume(TokenKind.ID)
        key = id_tok[1]
        next_tok = self.peek()
        if next_tok and next_tok[0] == TokenKind.COLON:
            self.consume(TokenKind.COLON)
            next_tok = self.peek()
        if not next_tok:
            raise SyntaxError('Unexpected EOF after key')
        if next_tok[0] == TokenKind.LBRACE:
            val = self.parse_message()
        elif next_tok[0] == TokenKind.LBRACKET:
            val = self.parse_list()
        elif next_tok[0] == TokenKind.STRING:
            val = self.consume(TokenKind.STRING)[1][1:-1]
        elif next_tok[0] == TokenKind.NUMBER:
            val = int(self.consume(TokenKind.NUMBER)[1])
        elif next_tok[0] == TokenKind.ID:
            val = self.consume(TokenKind.ID)[1]
            if val == 'true':
                val = True
            elif val == 'false':
                val = False
        else:
            raise SyntaxError(
                f"Unexpected token {next_tok[0]} ({next_tok[1]})")
        return key, val

    def parse_message(self) -> dict[str, Any]:
        """Parses a nested message enclosed in braces `{ ... }`.

        Returns:
            A dictionary representing the parsed message.

        Raises:
            SyntaxError: If the message is malformed or unexpected EOF is
                encountered.
        """
        self.consume(TokenKind.LBRACE)
        result = {}
        while True:
            tok = self.peek()
            if not tok:
                raise SyntaxError('Unexpected EOF in message')
            if tok[0] == TokenKind.RBRACE:
                self.consume(TokenKind.RBRACE)
                break
            key, val = self.parse_field()
            if key in result:
                if isinstance(result[key], list):
                    result[key].append(val)
                else:
                    result[key] = [result[key], val]
            else:
                result[key] = val
        return result

    def parse_list(self) -> list[Any]:
        """Parses a list of values enclosed in brackets `[ ... ]`.

        Returns:
            A list of parsed values.

        Raises:
            SyntaxError: If the list is malformed or unexpected EOF is
                encountered.
        """
        self.consume(TokenKind.LBRACKET)
        result = []
        while True:
            tok = self.peek()
            if not tok:
                raise SyntaxError('Unexpected EOF in list')
            if tok[0] == TokenKind.RBRACKET:
                self.consume(TokenKind.RBRACKET)
                break
            if tok[0] == TokenKind.STRING:
                result.append(self.consume(TokenKind.STRING)[1][1:-1])
            elif tok[0] == TokenKind.NUMBER:
                result.append(int(self.consume(TokenKind.NUMBER)[1]))
            elif tok[0] == TokenKind.ID:
                val = self.consume(TokenKind.ID)[1]
                if val == 'true':
                    result.append(True)
                elif val == 'false':
                    result.append(False)
                else:
                    result.append(val)
            else:
                raise SyntaxError(f"Unexpected token in list: {tok[0]}")
            next_tok = self.peek()
            if next_tok and next_tok[0] == TokenKind.COMMA:
                self.consume(TokenKind.COMMA)
            elif next_tok and next_tok[0] != TokenKind.RBRACKET:
                raise SyntaxError(
                    f"Expected COMMA or RBRACKET, got {next_tok[0]}")
        return result


def parse_text_proto(text: str) -> dict[str, Any]:
    return TextProtoParser(text).parse()
