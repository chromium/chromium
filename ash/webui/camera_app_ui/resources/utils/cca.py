#!/usr/bin/env python3
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Development script for CCA.

This tool bundles several scripts that are used for development only. See
`cca.py --help` for a list of commands.

Note that this should never be executed by any .gn file / by the real build
process of Chrome.
"""

import sys
from typing import List, Optional

from cca.commands import main as main_cmd


def main(argv: Optional[List[str]] = None) -> Optional[int]:
    return main_cmd.cmd.run(argv)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
