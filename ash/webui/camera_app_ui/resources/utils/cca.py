#!/usr/bin/env python3
# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys
from typing import List, Optional

from cca.commands import main as main_cmd


def main(argv: Optional[List[str]] = None) -> Optional[int]:
    return main_cmd.cmd.run(argv)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
