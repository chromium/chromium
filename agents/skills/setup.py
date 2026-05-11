#!/usr/bin/env vpython3

# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Installs and manages skills for the Gemini CLI."""

import argparse
import copy
import functools
import logging
import re
import os
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

_PROJECT_ROOT = Path(__file__).resolve().parents[2]
SKILL_DIRS = [
    _PROJECT_ROOT / 'agents' / 'internal' / 'skills',
    _PROJECT_ROOT / 'agents' / 'shared' / 'skills',
    _PROJECT_ROOT / 'agents' / 'skills',
    _PROJECT_ROOT / 'internal' / 'agents' / 'skills',
    _PROJECT_ROOT / 'third_party' / 'depot_tools' / 'agents' / 'skills'
]
sys.path.append(str(_PROJECT_ROOT))
from agents.common import gemini_helpers


@dataclass
class SkillInfo:
    """Holds information about a skill."""
    name: str
    enabled: bool = False
    location: str | None = None
    path: Path | None = None
    available: bool = False
    installed: bool = False


@functools.cache
def _get_gemini_cmd():
    return gemini_helpers.get_gemini_command(use_alias=True)


def get_installed_skills() -> dict[str, SkillInfo]:
    """Returns a dictionary of installed skills.

    Returns:
      A dictionary mapping skill names to SkillInfo objects.
    """
    gemini_cmd = _get_gemini_cmd()
    output = ''
    try:
        res = subprocess.run(gemini_cmd + ['skills', 'list', '--debug'],
                             stdout=subprocess.PIPE,
                             stderr=subprocess.STDOUT,
                             text=True,
                             check=True)
        output = res.stdout
    except subprocess.CalledProcessError as e:
        logging.info(' list error, %s', e)
        raise
    skills = {}
    # Regex to find each skill block.
    # It matches the skill name, status, description and location.
    skill_pattern = re.compile(
        r'^\s*([a-zA-Z0-9_-]+)\s+\[(Enabled|Disabled)\]\s*\n'
        r'\s*Description:\s+(.*?)\s*\n'
        r'\s*Location:\s+(.*)', re.MULTILINE)
    for match in skill_pattern.finditer(output):
        name, status, _, location = match.groups()
        # Strip the SKILLS.md file from the location
        location = Path(location).parent
        skills[name] = SkillInfo(name=name,
                                 enabled=(status == 'Enabled'),
                                 location=location.as_posix(),
                                 installed=True)
    return skills


def get_available_skills() -> dict[str, SkillInfo]:
    """Returns a list of available skills in the project.

    Returns:
      A dictionary mapping skill names to SkillInfo objects.
    """

    available_skills = {}
    for d in SKILL_DIRS:
        if d.exists():
            for skill in sorted(d.iterdir()):
                if skill.is_dir() and (skill / 'SKILL.md').exists():
                    available_skills[skill.name] = SkillInfo(name=skill.name,
                                                             available=True,
                                                             path=skill)
    return available_skills


def _shorten_path(path_str: str | Path | None) -> str:
    """Shortens a path string for display.

    Shortens $CHROMIUM_SRC to // and user home to ~
    """
    if path_str is None:
        return '-'

    path = Path(path_str)
    # Check if it's under project root
    try:
        if path.is_relative_to(_PROJECT_ROOT):
            relative_path = path.relative_to(_PROJECT_ROOT)
            return f'//{relative_path.as_posix()}'
    except ValueError:
        # Occurs on Windows if paths are on different drives.
        pass

    # Check if it's under user home
    try:
        home = Path.home()
        if path.is_relative_to(home):
            relative_path = path.relative_to(home)
            return f'~/{relative_path.as_posix()}'
    except (ValueError, RuntimeError):
        # ValueError on Windows for different drives.
        # RuntimeError if home directory cannot be determined.
        pass

    return path.as_posix()


def _print_skills_table(data: dict[str, SkillInfo]) -> None:
    """Prints a formatted table of skills."""
    headers = ['SKILL', 'AVAILABLE', 'INSTALLED', 'ENABLED', 'LOCATION']
    col_widths = {h: len(h) for h in headers}

    display_data = {}
    for name, skill_data in data.items():
        # Only show location for installed skills.
        location = '-'
        if skill_data.installed:
            location = _shorten_path(skill_data.location)

        display_data[name] = {
            'available': 'yes' if skill_data.available else 'no',
            'installed': 'yes' if skill_data.installed else 'no',
            'enabled': 'yes' if skill_data.enabled else 'no',
            'location': location
        }
        col_widths['SKILL'] = max(col_widths['SKILL'], len(name))
        col_widths['LOCATION'] = max(col_widths['LOCATION'], len(location))

    col_sep = '  '
    header_line = col_sep.join(h.ljust(col_widths[h]) for h in headers)
    print(header_line)
    print(col_sep.join('-' * col_widths[h] for h in headers))

    for name in sorted(data.keys()):
        row_data = display_data[name]
        row = [
            name.ljust(col_widths['SKILL']),
            row_data['available'].ljust(col_widths['AVAILABLE']),
            row_data['installed'].ljust(col_widths['INSTALLED']),
            row_data['enabled'].ljust(col_widths['ENABLED']),
            row_data['location'].ljust(col_widths['LOCATION']),
        ]
        print(col_sep.join(row))


def _run_skill_command(action: str, name_or_path: str | Path) -> None:
    """Runs a skill management command.

    Args:
      action: The action to perform (e.g., 'enable', 'disable').
      name_or_path: The name of the skill or the path to the skill directory.
    """
    is_path = isinstance(name_or_path, Path)
    target = name_or_path.as_posix() if is_path else name_or_path
    display_name = name_or_path.name if is_path else name_or_path
    gemini_cmd = _get_gemini_cmd()
    cmd = gemini_cmd + ['skills', action, target]

    action_labels = {
        'enable': 'Enabling',
        'disable': 'Disabling',
    }
    logging.info('  %s skill: %s',
                 action_labels.get(action, action.capitalize()), display_name)
    subprocess.run(cmd, capture_output=True, text=True, check=True)


def _handle_list(args: argparse.Namespace) -> bool:
    del args
    return list_skills()


def list_skills() -> bool:
    """Handles the 'list' command."""
    installed = get_installed_skills()
    available = get_available_skills()
    all_skills = copy.deepcopy(available)
    for name, info in installed.items():
        if name in all_skills:
            all_skills[name].installed = info.installed
            all_skills[name].enabled = info.enabled
            all_skills[name].location = info.location
        else:
            all_skills[name] = info

    _print_skills_table(all_skills)
    return True


def _handle_link(args: argparse.Namespace) -> bool:
    return link_skills(args.names)


def link_skills(names: list[str]) -> bool:
    """Handles the 'link' command."""
    available = get_available_skills()
    success = True
    for name in names:
        logging.info('Linking skill: %s', name)
        skill = available.get(name)
        if not skill:
            logging.error('Error: Skill "%s" not found.', name)
            success = False
            continue
        # Symlink to .agents/skills
        target_dir = _PROJECT_ROOT / '.agents' / 'skills'
        target_dir.mkdir(parents=True, exist_ok=True)
        try:
            os.symlink(skill.path,
                       _PROJECT_ROOT / '.agents' / 'skills' / skill.name, True)
        except FileExistsError:
            logging.info('Skill "%s" is already linked.', name)
    return success


def _handle_uninstall(args: argparse.Namespace) -> bool:
    return uninstall_skills(args.names)


def uninstall_skills(names: list[str]) -> bool:
    """Handles the 'uninstall' command."""
    installed = get_installed_skills()
    success = True
    for name in names:
        logging.info('Uninstalling skill: %s', name)
        skill = installed.get(name)
        if not skill:
            logging.info('No installed skill found named %s. Skipping', name)
            continue
        # Only unlink. Do not delete as it risks permanently losing files.
        if Path(skill.location).is_symlink():
            os.unlink(skill.location)
        else:
            try:
                _run_skill_command('uninstall', name)
            except subprocess.CalledProcessError as e:
                logging.error('Error: Failed to uninstall "%s": %s', name,
                              e.stderr.strip() if e.stderr else str(e))
                success = False
            except FileNotFoundError:
                success = False
    return success


def _handle_enable(args: argparse.Namespace) -> bool:
    return enable_disable_skills('enable', args.names)


def _handle_disable(args: argparse.Namespace) -> bool:
    return enable_disable_skills('disable', args.names)


def enable_disable_skills(action: str, names: list[str]) -> bool:
    """Handles 'enable' and 'disable' commands."""
    installed = get_installed_skills()
    success = True
    for name in names:
        if name not in installed:
            logging.error('Error: Skill "%s" is not linked. ', name)
            success = False
            continue
        try:
            _run_skill_command(action, name)
        except subprocess.CalledProcessError as e:
            logging.error('Error: Failed to %s "%s": %s', action, name,
                          e.stderr.strip() if e.stderr else str(e))
            success = False
        except FileNotFoundError:
            success = False
    return success


def main() -> None:
    """CLI for managing skills."""
    parser = argparse.ArgumentParser(description='Manage Gemini CLI skills.')
    subparsers = parser.add_subparsers(dest='command', help='Commands')

    list_parser = subparsers.add_parser('list', help='List skills')
    list_parser.set_defaults(func=_handle_list)

    # Common parser for commands that take skill names as arguments.
    names_parser = argparse.ArgumentParser(add_help=False)
    names_parser.add_argument('names', nargs='+', help='Names of the skills')

    for cmd, help_text, handler in [
        ('link', 'Link skills', _handle_link),
        ('uninstall', 'Uninstall skills', _handle_uninstall),
        ('enable', 'Enable skills', _handle_enable),
        ('disable', 'Disable skills', _handle_disable),
    ]:
        p = subparsers.add_parser(cmd, help=help_text, parents=[names_parser])
        p.set_defaults(func=handler)

    args = parser.parse_args()

    if not args.command:
        parser.print_help()
        sys.exit(1)

    logging.basicConfig(level=logging.INFO, format='%(message)s')

    try:
        success = args.func(args)
        if args.command != 'list':
            logging.info('Done')
        if not success:
            sys.exit(1)
    except subprocess.CalledProcessError as e:
        logging.error('Error: %s', e.stderr.strip() if e.stderr else str(e))
        sys.exit(1)
    except Exception as e:
        logging.error('Error: %s', e)
        sys.exit(1)


if __name__ == '__main__':
    main()
