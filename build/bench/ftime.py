#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Analyses the time spent on #including each file in a build.

Requires a full build to be run with the following in args.gn:
```
# Runs with -ftime-trace to profile every build action
compiler_timing = true
# Rust bindgen gets upset when you try and add -ftime-trace.
treat_warnings_as_errors = false
# crbug.com/462570809 (bug in clang causes occasional crashes when you use
# -ftime-trace with modules)
use_clang_modules = false
```
"""

from __future__ import annotations

import argparse
import dataclasses
import json
import multiprocessing
import pathlib

import utils

ftime_pb2 = utils.import_protobufs('ftime.proto')


# This is an optimization technique. path_to_id is expensive to copy, so we
# do it here to ensure it copies once per worker instead of once per job
def _initialize_worker(path_to_id):
  global PATH_TO_ID
  PATH_TO_ID = path_to_id


# Parsing has a lot of both i/o and computation, so we use smaller chunks.
_PARSE_CHUNKSIZE = 20
# Aggregation is very cheap and involves no I/O, so we want very large chunks.
_AGG_CHUNKSIZE = 100


def main(args):
  out = utils.CapacitorFile(args.out_file)

  traces = _collect_traces(args.out_dir, args.limit)

  # Mapping from filename to (ID, each time the source was seen)
  sources: dict[str, tuple[int, list[Source]]] = {}
  next_id = 0
  total = 0
  total_source = 0
  n_compiles = 0
  with multiprocessing.Pool() as p:
    print('Note: Parsing and aggregating can take several minutes')
    print('Parsing traces')
    parsed = p.imap_unordered(_parse_trace, traces, chunksize=_PARSE_CHUNKSIZE)
    for compile_total, compile_total_source, compile_sources in parsed:
      if compile_total_source is not None:
        total += compile_total
        total_source += compile_total_source
        n_compiles += 1
      for source in compile_sources:
        if source.name not in sources:
          sources[source.name] = (next_id, [])
          next_id += 1
        sources[source.name][1].append(source)

  print('Aggregating traces')
  path_to_id = {k: v[0] for k, v in sources.items()}
  with multiprocessing.Pool(initializer=_initialize_worker,
                            initargs=(path_to_id, )) as p:
    details = sorted(p.imap_unordered(_aggregate,
                                      (v[1] for v in sources.values()),
                                      chunksize=_AGG_CHUNKSIZE),
                     key=lambda x: x.id)

  print(f'Dumping to {out}')
  out.write(
      ftime_pb2.Analysis(
          out_dir=str(args.out_dir),
          total_us=total,
          total_source_us=total_source,
          n_compiles=n_compiles,
          sources=details,
      ))


@dataclasses.dataclass
class Source:
  """Represents a single compile of a single source file"""
  name: str
  transitive_us: int = 0
  duration_us: int = 0
  includes: list[Source] = dataclasses.field(default_factory=list)

  # Come up with a representation that isn't recursive on includes.
  def __repr__(self):
    return repr({
        'name': self.name,
        'transitive': self.transitive_us,
        'direct': self.direct_us,
        'includes': [i.name for i in self.includes],
    })


def _collect_traces(out_dir: pathlib.Path, limit: int | None = None):
  traces = []
  print('Finding json files')
  for d, _, filenames in out_dir.walk():
    all_files = set(filenames)
    for f in filenames:
      idx = f.find('.json')
      if idx > 0 and f[:idx] + '.o' in all_files:
        traces.append(d / f)

  if limit:
    # Sort for determinism, since that's useful when debugging.
    traces = sorted(traces)[:limit]
  print(f'Found {len(traces)} trace files.')
  return traces


def _aggregate(sources: list[Source]) -> ftime_pb2.SourceFile:
  """Aggregates many source files into one."""
  name = sources[0].name
  direct_us = 0
  transitive_us = 0
  includes = set()
  for source in sources:
    direct_us += source.direct_us
    transitive_us += source.transitive_us
    includes.update([PATH_TO_ID[s.name] for s in source.includes])
  return ftime_pb2.SourceFile(name=name,
                              id=PATH_TO_ID[name],
                              direct_us=direct_us,
                              transitive_us=transitive_us,
                              count=len(sources),
                              includes=sorted(includes))


def _parse_trace(path: pathlib.Path) -> tuple[int, int | None, list[Source]]:
  # Not every trace has a "Total Source".
  total_source = None
  events = []
  # Filter to only the events we care about.
  with path.open('r') as f:
    for event in json.load(f)['traceEvents']:
      match event['name']:
        case 'Source':
          events.append(event)
        case 'Total Source':
          total_source = event['dur']
        case 'Total ExecuteCompiler':
          total = event['dur']

  # Sort event by timestamp. Currently start and end events are paired next
  # to one another.
  # The sort is guaranteed to be stable.
  events.sort(key=lambda e: e['ts'])

  sources = []
  # The include stack. Contains sources, and their beginning timestamp.
  stack: list[tuple[Source, int]] = []
  for event in events:
    if event['ph'] == 'b':  # begin
      fname = event['args']['detail']
      source = Source(name=fname)
      sources.append(source)
      if stack:
        stack[-1][0].includes.append(source)
      stack.append((source, event['ts']))
    else:
      source, begin = stack.pop()
      # All timestamps in traces are measured in microseconds.
      duration = event['ts'] - begin
      source.transitive_us = duration
      for include in source.includes:
        duration -= include.transitive_us
      source.direct_us = duration

  return total, total_source, sources


if __name__ == '__main__':
  parser = argparse.ArgumentParser()
  parser.add_argument('-C',
                      required=True,
                      dest='out_dir',
                      help='GN output directory',
                      type=pathlib.Path)
  parser.add_argument('-o',
                      required=True,
                      dest='out_file',
                      help='capacitor file to output',
                      type=pathlib.Path)
  parser.add_argument(
      '--limit',
      type=int,
      help='Parse a limited number of traces. Useful for debugging.',
  )

  main(parser.parse_args())
