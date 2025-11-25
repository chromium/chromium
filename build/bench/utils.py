# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import importlib
import pathlib
import subprocess
import sys
import tempfile

_SRC = pathlib.Path(__file__).parent.parent.parent
_PROTO_DIR = _SRC / 'build/bench/protos'

# This ensures that we can import google.protobuf.
sys.path.append(str(_SRC / 'third_party/protobuf/python'))


def error(*args, **kwargs):
  print(*args, **kwargs, file=sys.stderr)
  exit(1)


def import_protobufs(fname: str):
  '''Imports the protobuf stored in protos/{fname}'''
  assert fname.endswith('.proto')
  path = _PROTO_DIR / fname
  with tempfile.TemporaryDirectory() as d:
    subprocess.run(
        [
            'protoc',
            f'--python_out={d}',
            f'--proto_path={_PROTO_DIR}',
            str(path),
        ],
        check=True,
    )

    sys.path.append(d)
    mod = importlib.import_module(f'{path.stem}_pb2')
    sys.path.pop()
    return mod


# We always use a capacitor file because it's self-describing, thus it can be
# directly queried without having to appear in the protodb.
class CapacitorFile:
  """A capacitor file to write to"""

  def __init__(self, out: pathlib.Path):
    # Do the validation in __init__ to prevent running a really long analysis,
    # then failing beacuse of something we could have known from the start.
    if out.suffix != '.capacitor':
      error(f'Output file must be a capacitor file. Got {out}')
    if not out.parent.resolve().is_dir():
      error(f'{out.parent} is not a directory')
    self.out = out

  def write(self, obj):
    """Writes a proto message to a capacitor file."""
    cmd = [
        'gqui',
        'from',
        'rawproto:-',
        'proto',
        f'{obj.DESCRIPTOR.file.package}.{obj.__class__.__name__}',
        f'--protofiles={_PROTO_DIR / obj.DESCRIPTOR.file.name}',
        f'--outfile=capacitor:{self.out}',
    ]
    subprocess.run(
        cmd,
        check=True,
        input=obj.SerializeToString(),
    )

  def __str__(self):
    return str(self.out)

  def __repr__(self):
    return repr(self.out)
