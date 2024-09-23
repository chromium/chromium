# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
""" The module to create and manage ExceptionOccurrence records. """

import os
import json
import traceback

from abc import ABC, abstractmethod
from typing import List

from google.protobuf import any_pb2
from google.protobuf.json_format import MessageToDict

from lib.proto.exception_occurrences_pb2 import ExceptionOccurrence
from lib.proto.exception_occurrences_pb2 import ExceptionOccurrences

# This is used as the key when being uploaded to ResultDB via result_sink
# and shouldn't be changed
EXCEPTION_OCCURRENCES_KEY = 'exception_occurrences'

# This is used as the key when being uploaded to ResultDB via rdb
# and shouldn't be changed
EXCEPTION_OCCURRENCES_FILENAME = f'{EXCEPTION_OCCURRENCES_KEY}.jsonpb'

_records: List[ExceptionOccurrence] = []


class Formatter(ABC):

  @abstractmethod
  def format_name(self, exc: Exception) -> str:
    """Format the exception name."""

  @abstractmethod
  def format_stacktrace(self, exc: Exception) -> List[str]:
    """Format the exception stacktrace."""


class _Formatter(Formatter):

  def format_name(self, exc: Exception) -> str:
    exc_name = type(exc).__qualname__
    exc_module = type(exc).__module__
    if exc_module not in ('__main__', 'builtins'):
      exc_name = '%s.%s' % (exc_module, exc_name)
    return exc_name

  def format_stacktrace(self, exc: Exception) -> List[str]:
    return traceback.format_exception(type(exc), exc, exc.__traceback__)


# Default formatter
_default_formatter = _Formatter()

def _record_time(exc: ExceptionOccurrence):
  exc.occurred_time.GetCurrentTime()

def register(exc: Exception,
             formatter: Formatter = _default_formatter) -> ExceptionOccurrence:
  """Create and register an ExceptionOccurrence record."""
  ret = ExceptionOccurrence(name=formatter.format_name(exc),
                            stacktrace=formatter.format_stacktrace(exc))
  _record_time(ret)
  _records.append(ret)
  return ret


def size() -> int:
  """Get the current size of registered ExceptionOccurrence records."""
  return len(_records)


def clear() -> None:
  """Clear all the registered ExceptionOccurrence records."""
  _records.clear()


def to_dict() -> dict:
  """Convert all the registered ExceptionOccurrence records to an dict.

  The records are wrapped in protobuf Any message before exported as dict
  so that an additional key "@type" is included.
  """
  occurrences = ExceptionOccurrences()
  occurrences.datapoints.extend(_records)
  any_msg = any_pb2.Any()
  any_msg.Pack(occurrences)
  return MessageToDict(any_msg, preserving_proto_field_name=True)


def to_json() -> str:
  """Convert all the registered ExceptionOccurrence records to a json str."""
  return json.dumps(to_dict(), sort_keys=True, indent=2)


def dump(dir_path: str) -> None:
  """Dumps the records into |EXCEPTION_OCCURRENCES_FILENAME| in the |path|."""
  os.makedirs(dir_path, exist_ok=True)
  with open(os.path.join(dir_path, EXCEPTION_OCCURRENCES_FILENAME),
            'w',
            encoding='utf-8') as wf:
    wf.write(to_json())
