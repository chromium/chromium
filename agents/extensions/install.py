#!/usr/bin/env vpython3

# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Installs and manages configurations for extensions for the Gemini CLI.

This script is a wrapper around the `gemini extensions` commands.
"""

import argparse
import json
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

_PROJECT_ROOT = Path(__file__).resolve().parents[2]
sys.path.append(str(_PROJECT_ROOT))
from agents.common import gemini_helpers


IGNORED_EXTENSIONS = ['example-server']


@dataclass
class ExtensionInfo:
    """Holds information about an extension."""
    name: str
    available: str = '-'
    installed: str = '-'
    linked: bool = False
    enabled_for_workspace: bool = False


def _get_extension_version(extension_path: Path) -> str:
    """Returns the version of the extension from its manifest file."""
    manifest_path = extension_path / 'gemini-extension.json'
    if not manifest_path.exists():
        return '-'
    with open(manifest_path, encoding='utf-8') as f:
        try:
            data = json.load(f)
            return data.get('version', '-')
        except json.JSONDecodeError:
            return '-'


def _get_available_extensions(
        project_root: Path | None,
        extra_extensions_dirs: list[Path] | None) -> dict[str, ExtensionInfo]:
    """Returns a dictionary of available extensions."""
    data = {}
    dirs = get_extensions_dirs(project_root,
                               extra_extensions_dirs=extra_extensions_dirs)
    for extensions_dir in dirs:
        for name in get_extensions_from_dir(extensions_dir):
            if name in IGNORED_EXTENSIONS:
                continue
            path = extensions_dir / name
            version = _get_extension_version(path)
            data[name] = ExtensionInfo(name=name, available=version)
    return data


def _parse_installed_extensions_output(
        output: str) -> dict[str, ExtensionInfo]:
    """Parses the output of `gemini extensions list` and returns a dictionary of
    installed extension details.
    """
    data: dict[str, ExtensionInfo] = {}
    current_name = None
    for line in output.splitlines():
        is_indented = line.lstrip() != line
        if not is_indented:
            # This is a name/version line
            parts = line.split()
            if (len(parts) >= 2 and parts[-1].startswith('(')
                    and parts[-1].endswith(')')):
                name = parts[-2]
                version = parts[-1].strip('()')
                current_name = name
                data[current_name] = ExtensionInfo(
                    name=name,
                    installed=version,
                )
        elif current_name and is_indented:
            # This is a detail line
            stripped_line = line.strip()
            current_ext = data[current_name]
            if (stripped_line.startswith('Source:')
                    and '(Type: link)' in stripped_line):
                current_ext.linked = True
            elif (stripped_line.startswith('Enabled (Workspace):')
                  and 'true' in stripped_line):
                current_ext.enabled_for_workspace = True

    return data


def _print_extensions_table(data: dict[str, ExtensionInfo]) -> None:
    """Prints a formatted table of extensions."""
    headers = ['EXTENSION', 'AVAILABLE', 'INSTALLED', 'LINKED', 'ENABLED']
    col_widths = {h: len(h) for h in headers}
    for name, ext_data in data.items():
        col_widths['EXTENSION'] = max(col_widths['EXTENSION'], len(name))
        col_widths['AVAILABLE'] = max(col_widths['AVAILABLE'],
                                      len(ext_data.available))
        col_widths['INSTALLED'] = max(col_widths['INSTALLED'],
                                      len(ext_data.installed))
        col_widths['ENABLED'] = max(
            col_widths['ENABLED'],
            len('workspace') if ext_data.enabled_for_workspace else 0)

    col_sep = '  '
    header_line = col_sep.join(h.ljust(col_widths[h]) for h in headers)
    print(header_line)
    print(col_sep.join('-' * col_widths[h] for h in headers))

    for name in sorted(data.keys()):
        ext_data = data[name]
        row = [
            name.ljust(col_widths['EXTENSION']),
            ext_data.available.ljust(col_widths['AVAILABLE']),
            ext_data.installed.ljust(col_widths['INSTALLED']),
            ('yes' if ext_data.linked else 'no').ljust(col_widths['LINKED']),
            ('workspace' if ext_data.enabled_for_workspace else '-').ljust(
                col_widths['ENABLED']),
        ]
        print(col_sep.join(row))


def _handle_list_command(project_root: Path | None,
                         extra_extensions_dirs: list[Path]) -> None:
    """Shows all available and installed extensions."""
    gemini_cmd = gemini_helpers.get_gemini_executable()
    all_data = _get_available_extensions(project_root, extra_extensions_dirs)

    # Get installed extensions
    result = subprocess.run([gemini_cmd, 'extensions', 'list'],
                            capture_output=True,
                            text=True,
                            check=True)
    installed_data = _parse_installed_extensions_output(result.stdout)

    for name, data in installed_data.items():
        if name not in all_data:
            all_data[name] = data
        else:
            all_data[name].installed = data.installed
            all_data[name].linked = data.linked
            all_data[name].enabled_for_workspace = data.enabled_for_workspace

    _print_extensions_table(all_data)


def get_extensions_from_dir(extensions_dir: Path) -> list[str]:
    """Returns a list of all extensions in the given directory.

    Args:
        extensions_dir: The directory containing the extensions configurations.

    Returns:
        A list of extension names.
    """
    if not extensions_dir.exists():
        return []
    return [
        p.parent.name for p in extensions_dir.glob('*/gemini-extension.json')
    ]


def get_project_root() -> Path:
    """Gets the project root."""
    return _PROJECT_ROOT


def get_extensions_dirs(
        project_root: Path | None,
        extra_extensions_dirs: list[Path] | None = None) -> list[Path]:
    """Returns a list of all extension directories."""
    if not project_root:
        return []

    extensions_dirs = []
    if extra_extensions_dirs:
        extensions_dirs.extend(extra_extensions_dirs)

    primary_extensions_dir = project_root / 'agents' / 'extensions'
    if primary_extensions_dir.exists():
        extensions_dirs.append(primary_extensions_dir)

    internal_extensions_dir = (project_root / 'internal' / 'agents' /
                               'extensions')
    if internal_extensions_dir.exists():
        extensions_dirs.append(internal_extensions_dir)
    return extensions_dirs


def find_extensions_dir_for_extension(
        extension_name: str, extensions_dirs: list[Path]) -> Path | None:
    """Finds the extensions directory for a given extension."""
    for extensions_dir in extensions_dirs:
        if (extensions_dir / extension_name).exists():
            return extensions_dir
    return None


def get_local_extension_dir(project_root: Path | None) -> Path | None:
    """Returns the local Gemini CLI extension directory."""
    if project_root:
        return project_root / '.gemini' / 'extensions'
    return None


def get_global_extension_dir() -> Path:
    """Returns the Gemini CLI extension directory."""
    return Path.home() / '.gemini' / 'extensions'


def _run_command(command: list[str], skip_prompt: bool = False) -> None:
    """Runs a command and handles errors."""
    try:
        if skip_prompt:
            subprocess.run(command, check=True, input='y\n', encoding='utf-8')
        else:
            subprocess.run(command, check=True)
    except FileNotFoundError:
        print(
            f"Error: Command '{command[0]}' not found. Is 'gemini' in your "
            'PATH?',
            file=sys.stderr,
        )
        sys.exit(1)
    except subprocess.CalledProcessError as e:
        sys.exit(e.returncode)


def fix_extensions(project_root: Path | None) -> None:
    """Migrates deprecated project-level extensions to the new user model.

    This is a one-time migration tool to move from the old model of
    project-level extensions (stored in `<project>/.gemini/extensions`) to the
    new model of user-level extensions (stored in `~/.gemini/extensions`) that
    can be enabled/disabled on a per-workspace basis.

    The function will:
    1. Scan for a `.gemini/extensions` directory in the project root.
    2. For each extension found, it will make a link in the user-level folder.
    3. It will then disable the extension at the user level.
    4. It will then enable the extension for the current workspace.
    5. Finally, it will remove the old project-level directory.
    """
    if not project_root:
        print('Error: Could not determine project root.', file=sys.stderr)
        sys.exit(1)

    project_extensions_dir = get_local_extension_dir(project_root)
    if not project_extensions_dir or not project_extensions_dir.exists():
        print('No project-level extensions found to fix.')
        return

    extensions = get_extensions_from_dir(project_extensions_dir)
    if not extensions:
        print('No valid project-level extensions found. Removing empty or '
              'invalid project-level extensions directory.')
        shutil.rmtree(project_extensions_dir)
        return

    user_extensions_dir = get_global_extension_dir()
    source_dirs = get_extensions_dirs(project_root)

    gemini_cmd = gemini_helpers.get_gemini_executable()
    print('Found project-level extensions. Converting to the new model...')
    for extension in extensions:
        if (user_extensions_dir / extension).exists():
            print(
                f'Warning: User extension "{extension}" already exists. '
                'Skipping installation to avoid overwriting.',
                file=sys.stderr,
            )
            continue

        source_dir_for_ext = find_extensions_dir_for_extension(
            extension, source_dirs)
        if not source_dir_for_ext:
            print(
                f'Warning: Source for extension "{extension}" not found. '
                'Skipping.',
                file=sys.stderr,
            )
            continue

        print(f'Fixing "{extension}"...')
        _run_command([
            gemini_cmd, 'extensions', 'link',
            str(source_dir_for_ext / extension)
        ])
        _run_command(
            [gemini_cmd, 'extensions', 'disable', extension, '--scope=User'])
        _run_command([
            gemini_cmd, 'extensions', 'enable', extension, '--scope=Workspace'
        ])

    print('Removing old project-level extensions directory...')
    shutil.rmtree(project_extensions_dir)
    print('Done.')


def _check_for_workspace_extensions(project_root: Path | None) -> None:
    """Prompts the user to run the fix command if necessary."""
    if not project_root:
        return
    project_extensions_dir = get_local_extension_dir(project_root)
    if project_extensions_dir and project_extensions_dir.exists():
        print(
            'WARNING: Project-level extensions are deprecated. Please run '
            "'install.py fix' to migrate to the new user-level model.\n",
            file=sys.stderr)


def check_gemini_version() -> None:
    """Checks if the Gemini CLI version is sufficient."""
    required_version = (0, 8, 0)
    version_str = gemini_helpers.get_gemini_version()
    if not version_str:
        print(
            'Error: Could not determine Gemini CLI version. Please ensure '
            "'gemini' is in your PATH and working correctly.",
            file=sys.stderr,
        )
        sys.exit(1)
    try:
        version_tuple = tuple(map(int, version_str.split('.')))
    except ValueError:
        print(
            f'Error: Could not parse Gemini CLI version: {version_str}',
            file=sys.stderr,
        )
        sys.exit(1)
    if version_tuple < required_version:
        print(
            f'Error: Gemini CLI version {version_str} is too old. Version '
            f'>={".".join(map(str, required_version))} is required.',
            file=sys.stderr,
        )
        sys.exit(1)


def main() -> None:
    """Installs and manages extension."""
    check_gemini_version()
    project_root = get_project_root()
    _check_for_workspace_extensions(project_root)

    parser = argparse.ArgumentParser(
        description='Install and manage extensions.')
    parser.add_argument(
        '--extra-extensions-dir',
        action='append',
        type=Path,
        default=[],
        help='Path to a directory containing extensions. Can be specified '
        'multiple times.',
    )
    subparsers = parser.add_subparsers(
        dest='command',
        help='Available commands.',
        description=('Install and manage extensions. To get help for a '
                     'specific command, run "install.py <command> -h".'))

    add_parser = subparsers.add_parser(
        'add', help='Add new extension (links by default).')
    add_parser.add_argument(
        '--copy',
        action='store_true',
        help='Use directory copies rather than links.',
    )
    add_parser.add_argument(
        '--skip-prompt',
        action='store_true',
        help='Skip any interactive prompts.',
    )
    add_parser.add_argument(
        'extensions',
        nargs='+',
        help='A list of extension directory names to add.',
    )

    update_parser = subparsers.add_parser('update', help='Update extensions.')
    update_parser.add_argument(
        '--skip-prompt',
        action='store_true',
        help='Skip any interactive prompts.',
    )
    update_parser.add_argument(
        'extensions',
        nargs='*',
        help=('A list of extension directory names to update. If not '
              'specified, all installed extensions will be updated.'))

    remove_parser = subparsers.add_parser('remove', help='Remove extensions.')
    remove_parser.add_argument(
        'extensions',
        nargs='+',
        help='A list of extension directory names to remove.')

    subparsers.add_parser('list',
                          help='List all available and installed extensions.')
    subparsers.add_parser(
        'fix',
        help='Fix project-level extensions to follow the new model.',
    )

    args = parser.parse_args()

    if not args.command:
        parser.print_help()
        sys.exit(1)

    gemini_cmd = gemini_helpers.get_gemini_executable()

    if args.command == 'list':
        _handle_list_command(project_root, args.extra_extensions_dir)
        return

    if args.command == 'fix':
        fix_extensions(project_root)
        return

    extensions_to_process = args.extensions
    if not extensions_to_process and args.command == 'update':
        _run_command([gemini_cmd, 'extensions', 'update', '--all'])
        return

    for extension in extensions_to_process:
        if args.command == 'add':
            source_dirs = get_extensions_dirs(
                project_root, extra_extensions_dirs=args.extra_extensions_dir)
            source_dir = find_extensions_dir_for_extension(
                extension, source_dirs)
            if not source_dir:
                print(f"Error: Extension '{extension}' not found.",
                      file=sys.stderr)
                sys.exit(1)
            cmd = [gemini_cmd, 'extensions']
            if args.copy:
                cmd.extend(['install', str(source_dir / extension)])
            else:
                cmd.extend(['link', str(source_dir / extension)])
            _run_command(cmd, skip_prompt=args.skip_prompt)
        elif args.command == 'update':
            _run_command([gemini_cmd, 'extensions', 'update', extension],
                         skip_prompt=args.skip_prompt)
        elif args.command == 'remove':
            if '_' in extension:
                # gemini rejects extension names with _ in them so if they're
                # already installed we need to delete them directly
                try:
                    shutil.rmtree(get_global_extension_dir() / extension)
                except OSError as e:
                    print(f"Error removing extension '{extension}': {e}",
                          file=sys.stderr)
                    sys.exit(1)
            else:
                _run_command(
                    [gemini_cmd, 'extensions', 'uninstall', extension])


if __name__ == '__main__':
    main()
