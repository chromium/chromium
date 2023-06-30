# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
from typing import Dict, Set, Tuple
import xml.sax.handler

from cca import cli

_RESOURCES_H_PATH = "../resources.h"
_I18N_STRING_TS_PATH = "./js/i18n_string.ts"
_CAMERA_STRINGS_GRD_PATH = "./strings/camera_strings.grd"


def _parse_resources_h() -> Set[Tuple[str, str]]:
    with open(_RESOURCES_H_PATH, "r") as f:
        content = f.read()
        return set(re.findall(r'\{"(\w+)",\s*(\w+)\}', content))


def _parse_i18n_string_ts() -> Dict[str, str]:
    with open(_I18N_STRING_TS_PATH, "r") as f:
        content = f.read()
        return dict(re.findall(r"(\w+) =\s*'(\w+)'", content))


# Same as tools/check_grd_for_unused_strings.py
class _GrdIDExtractor(xml.sax.handler.ContentHandler):
    """Extracts the IDs from messages in GRIT files"""
    def __init__(self):
        self.id_set_: Set[str] = set()

    def startElement(self, name: str, attrs: Dict[str, str]):
        if name == "message":
            self.id_set_.add(attrs["name"])

    def allIDs(self):
        """Return all the IDs found"""
        return self.id_set_.copy()


def _parse_camera_strings_grd() -> Set[str]:
    handler = _GrdIDExtractor()
    xml.sax.parse(_CAMERA_STRINGS_GRD_PATH, handler)
    return handler.allIDs()


@cli.command(
    "check-strings",
    help="check string resources",
    description="""Ensure files related to string resources are having the
        same strings. This includes resources.h,
        resources/strings/camera_strings.grd and
        resources/js/i18n_string.ts.""",
)
def cmd() -> int:
    returncode = 0

    def check_name_id_consistent(strings: Set[Tuple[str, str]], filename: str):
        nonlocal returncode
        bad = [(name, id) for (name, id) in strings
               if id != f"IDS_{name.upper()}"]
        if bad:
            print(f"{filename} includes string id with inconsistent name:")
            for (name, id) in bad:
                print(f"    {name}: Expect IDS_{name.upper()}, got {id}")
            returncode = 1

    def check_all_ids_exist(all_ids: Set[str], ids: Set[str], filename: str):
        nonlocal returncode
        missing = all_ids.difference(ids)
        if missing:
            print(f"{filename} is missing the following string id:")
            print(f'    {", ".join(sorted(missing))}')
            returncode = 1

    def check_all_name_lower_case(names: Set[str], filename: str):
        nonlocal returncode
        hasUpper = [name for name in names if not name.islower()]
        if hasUpper:
            print(f"{filename} includes string name with upper case:")
            for name in hasUpper:
                print(f"    Incorrect name: {name}")
            returncode = 1

    def check_unused(i18n_string_ts_dict: Dict[str, str]):
        nonlocal returncode
        cca_root = os.getcwd()
        name_set_from_html_files = set()
        id_set_from_ts_files = set()

        with open(os.path.join(cca_root, "views/main.html")) as f:
            # Find all values of i18n-xxx attributes such as `i18n-text="name"`.
            name_set_from_html_files.update(
                re.findall(r"i18n-[\w-]+=\"(\w+)\"", f.read()))

        for dirpath, _dirnames, filenames in os.walk(
                os.path.join(cca_root, "js")):
            for filename in filenames:
                if not filename.endswith(".ts"):
                    continue
                with open(os.path.join(dirpath, filename)) as f:
                    id_set_from_ts_files.update(
                        re.findall(r"I18nString\.(\w+)", f.read()))

        unused_ids = [
            id for (id, name) in i18n_string_ts_dict.items()
            if id not in id_set_from_ts_files
            and name not in name_set_from_html_files
        ]

        unused_ids = []
        for (id, name) in i18n_string_ts_dict.items():
            if id in id_set_from_ts_files or name in name_set_from_html_files:
                continue
            unused_ids.append(id)

        if len(unused_ids) > 0:
            print("The following strings are defined in i18n_string.ts but "
                  "unused. Please remove them:")
            for id in unused_ids:
                print(f"    {id}")
            returncode = 1

    resources_h_strings = _parse_resources_h()
    check_name_id_consistent(resources_h_strings, _RESOURCES_H_PATH)
    resources_h_ids = set([id for (name, id) in resources_h_strings])

    i18n_string_ts_dict = _parse_i18n_string_ts()
    check_unused(i18n_string_ts_dict)

    i18n_string_ts_name_id_set = set([
        (name, f"IDS_{id}") for (id, name) in i18n_string_ts_dict.items()
    ])
    check_name_id_consistent(i18n_string_ts_name_id_set, _I18N_STRING_TS_PATH)
    i18n_string_ts_ids = set([id for (name, id) in i18n_string_ts_name_id_set])

    resources_h_names = set([name for (name, id) in resources_h_strings])
    check_all_name_lower_case(resources_h_names, _RESOURCES_H_PATH)

    i18n_string_ts_names = set(
        [name for (name, id) in i18n_string_ts_name_id_set])
    check_all_name_lower_case(i18n_string_ts_names, _I18N_STRING_TS_PATH)

    camera_strings_grd_ids = _parse_camera_strings_grd()

    all_ids = resources_h_ids.union(i18n_string_ts_ids, camera_strings_grd_ids)

    check_all_ids_exist(all_ids, resources_h_ids, _RESOURCES_H_PATH)
    check_all_ids_exist(all_ids, i18n_string_ts_ids, _I18N_STRING_TS_PATH)
    check_all_ids_exist(all_ids, camera_strings_grd_ids,
                        _CAMERA_STRINGS_GRD_PATH)

    return returncode
