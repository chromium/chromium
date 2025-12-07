# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import re
import typing
from typing import Optional
from xml.dom import minidom

from cra import cli
from cra import util


def _get_grd_name(id: str) -> str:
    return f"IDS_RECORDER_{id}"


def _add_string_to_grdp(id: str) -> bool:
    """Adds a string to grdp file.

    Returns False if the id already existed in the grd.
    """
    grd_name = _get_grd_name(id)

    grd_path = util.get_strings_dir() / "recorder_strings.grdp"
    dom = minidom.parse(str(grd_path))
    messages = dom.getElementsByTagName("grit-part")[0]

    def find_next_sibling(name: str):
        """Find the next sibling to insert the new string."""
        for message in messages.childNodes:
            if message.nodeType != minidom.Node.ELEMENT_NODE:
                continue
            name = message.getAttribute("name")
            if name >= grd_name:
                return message
        return None

    next_sibling = find_next_sibling(grd_name)
    if next_sibling is not None and next_sibling.getAttribute(
            "name") == grd_name:
        return False

    new_message = dom.createElement('message')
    new_message.setAttribute('desc', '')
    new_message.setAttribute('name', grd_name)
    new_message.appendChild(dom.createTextNode("\n  "))
    messages.insertBefore(new_message, next_sibling)
    if next_sibling is None:
        # This is the last element, so there's no indent before next element
        messages.insertBefore(dom.createTextNode("  "), new_message)
        messages.insertBefore(dom.createTextNode("\n"), next_sibling)
    else:
        messages.insertBefore(dom.createTextNode("\n  "), next_sibling)

    # The encoding='UTF-8' is for adding xml header.
    xml = dom.toxml(encoding='UTF-8').decode() + "\n"
    # Make the header looks a bit better.
    xml = xml.replace('encoding="UTF-8"?>',
                      'encoding="UTF-8"?>\n').replace('<grit-part>',
                                                      '\n\n<grit-part>')

    with open(grd_path, "w") as grd_file:
        grd_file.write(xml)
    return True


def _add_string_to_resource_h(id: str):
    cra_root = util.get_cra_root()

    resource_header_path = cra_root.parent / "resources.h"
    with open(resource_header_path, "r") as f:
        resource_header = f.read()

    def handle_replace(match: re.Match) -> str:
        content = typing.cast(str, match[1])
        pairs = re.findall(r'\{"(\w+)",\s*(\w+)\}', content)
        pairs.append((util.to_camel_case(id), _get_grd_name(id)))
        pairs.sort()
        return ''.join(f'{{"{cra_id}", {grd_id}}},\n'
                       for cra_id, grd_id in pairs)

    resource_header = re.sub(r'(?<=kLocalizedStrings\[\] = \{)(.*)(?=\};)',
                             handle_replace,
                             resource_header,
                             flags=re.DOTALL)
    with open(resource_header_path, "w") as f:
        f.write(resource_header)
    util.run(["clang-format", "-i", str(resource_header_path)], cwd=cra_root)


def _add_string_to_i18n_ts(id: str):
    cra_root = util.get_cra_root()

    i18n_ts_path = cra_root / "core/i18n.ts"
    with open(i18n_ts_path, "r") as f:
        i18n_ts = f.read()

    def handle_replace(match: re.Match) -> str:
        content = typing.cast(str, match[1])
        ids = [id.strip() for id in content.split(',')]
        ids = [id for id in ids if id]
        ids.append(f"'{util.to_camel_case(id)}'")
        ids.sort()
        return ''.join(f'  {id},\n' for id in ids)

    i18n_ts = re.sub(r'(?<=noArgStringNames = \[\n)(.*?)(?=\])',
                     handle_replace,
                     i18n_ts,
                     flags=re.DOTALL)
    with open(i18n_ts_path, "w") as f:
        f.write(i18n_ts)


def _add_string(id: str) -> Optional[str]:
    logging.debug(f"Adding {id}")

    if not re.match(r"^[A-Z0-9_]+$", id):
        return "ID should be in CAMEL_CASE"

    if id.startswith("IDS_") or id.startswith("RECORDER_"):
        return "ID should not have IDS_RECORDER_ prefix"

    if not _add_string_to_grdp(id):
        logging.info(f"{id} already exists, skipping.")
        return

    _add_string_to_resource_h(id)
    _add_string_to_i18n_ts(id)

    return


@cli.command("add-strings",
             help="add new string entries",
             description="add new string entries to recorder app")
@cli.option("ids",
            metavar="ID",
            nargs="+",
            help="ID for the strings to be added. "
            "Should be in CAPITAL_CASE without IDS_RECORDER_ prefix.")
def cmd(ids: list[str]) -> int:
    for id in ids:
        err = _add_string(id)
        if err is not None:
            logging.error(f"Error when trying to add {id}: {err}")

    print("All strings added.\n"
          "Next steps:\n"
          "  * In /chromeos/strings/recorder_strings.grdp,"
          " add description and the text to newly added entries.\n"
          "  * In core/i18n.ts, if any string use arguments ($1, $2, ...),"
          " move those ID from noArgStrings to withArgsStrings"
          " and annotate with correct argument types.")

    return 0
