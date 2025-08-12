#!/usr/bin/env vpython3

# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Installs and manages configurations for extensions for the Gemini CLI.

This script allows you to install extensions from the 'agents/extensions'
directory into the Gemini CLI extensions directory. You can install
configurations at the project level (in the '.gemini/extensions' folder at the
root of the repository) or globally (in '~/.gemini/extensions').
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys


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


def get_project_root() -> Path | None:
    """Gets the project root using `gclient root`."""
    try:
        gclient_root = subprocess.check_output(
            ['gclient', 'root'], encoding='utf-8'
        ).strip()
        if gclient_root:
            project_root = Path(gclient_root) / 'src'
            if project_root.is_dir():
                return project_root
    except (subprocess.CalledProcessError, FileNotFoundError):
        print(
            "Error: Could not determine project root. Please ensure 'gclient' "
            "is in your PATH and that you are in a gclient project.",
            file=sys.stderr)
    return None


def get_extensions_dirs(project_root: Path | None) -> list[Path]:
    """Returns a list of all extension directories."""
    if not project_root:
        return []

    extensions_dirs = []
    primary_extensions_dir = project_root / 'agents' / 'extensions'
    if primary_extensions_dir.exists():
        extensions_dirs.append(primary_extensions_dir)

    internal_extensions_dir = (project_root / 'internal' / 'agents' /
                               'extensions')
    if internal_extensions_dir.exists():
        extensions_dirs.append(internal_extensions_dir)
    return extensions_dirs


def find_extensions_dir_for_extension(
        extension_name: str,
        extensions_dirs: list[Path]
    ) -> Path | None:
    """Finds the extensions directory for a given extension."""
    for extensions_dir in extensions_dirs:
        if (extensions_dir / extension_name).exists():
            return extensions_dir
    return None


def get_extension_dir(project_root: Path | None,
                        use_global: bool = False) -> Path | None:
    """Returns the Gemini CLI extension directory."""
    if use_global:
        return Path.home() / '.gemini' / 'extensions'
    if project_root:
        return project_root / '.gemini' / 'extensions'
    return None


def get_installed_extensions(extensions_dir: Path) -> list[str]:
    """Returns a list of all installed extensions.

    Args:
        extensions_dir: The extension directory to search.

    Returns:
        A list of installed extension names.
    """
    if not extensions_dir.exists():
        return []
    return [
        p.parent.name for p in extensions_dir.glob('*/gemini-extension.json')
    ]


def get_extension_version(extension_path: Path) -> str:
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


def get_dir_hash(directory: Path) -> bytes | None:
    """Calculates a hash for the contents of a directory."""
    hashes = []
    files_to_hash = []
    for root, dirs, files in os.walk(directory):
        # We do not want changes to test-only data to count as a change to
        # the extension.
        if 'tests' in dirs:
            dirs.remove('tests')
        for name in files:
            files_to_hash.append(os.path.join(root, name))

    for path in sorted(files_to_hash):
        try:
            hashes.append(
                subprocess.check_output(['git', 'hash-object',
                                         str(path)],
                                        stderr=subprocess.DEVNULL).strip())
        except (subprocess.CalledProcessError, FileNotFoundError):
            # Fallback for non-git environments
            hasher = hashlib.sha1()
            with open(path, 'rb') as f:
                while chunk := f.read(8192):
                    hasher.update(chunk)
            hashes.append(hasher.hexdigest().encode('utf-8'))

    if not hashes:
        return None

    hasher = subprocess.run(['git', 'hash-object', '--stdin'],
                            input=b'\n'.join(hashes),
                            capture_output=True,
                            check=False)
    if hasher.returncode == 0:
        return hasher.stdout.strip()
    else:
        # Fallback for non-git environments
        hasher = hashlib.sha1()
        for h in hashes:
            hasher.update(h)
        return hasher.hexdigest().encode('utf-8')


def is_up_to_date(extension_name: str, source_extensions_dir: Path,
                  target_extensions_dir: Path) -> bool:
    """Checks if the installed extension configuration is up to date."""
    source_dir = source_extensions_dir / extension_name
    dest_dir = target_extensions_dir / extension_name

    if not dest_dir.exists():
        return False

    source_hash = get_dir_hash(source_dir)
    dest_hash = get_dir_hash(dest_dir)

    return source_hash == dest_hash


def list_extensions(project_root: Path | None,
                      extensions_dirs: list[Path]) -> None:
    """Lists all available and installed extensions."""
    # Get available, local, and global extensions
    available_extensions = {}
    for extensions_dir in extensions_dirs:
        for name in get_extensions_from_dir(extensions_dir):
            available_extensions[name] = get_extension_version(
                extensions_dir / name)

    local_extensions_dir = get_extension_dir(project_root, use_global=False)
    if local_extensions_dir:
        local_extensions = {
            name: get_extension_version(local_extensions_dir / name)
            for name in get_installed_extensions(local_extensions_dir)
        }

    global_extensions_dir = get_extension_dir(project_root, use_global=True)
    if global_extensions_dir:
        global_extensions = {
            name: get_extension_version(global_extensions_dir / name)
            for name in get_installed_extensions(global_extensions_dir)
        }

    all_extension_names = sorted(
        (set(available_extensions)
         | set(local_extensions)
         | set(global_extensions)) - {'example_server'})

    # Print table
    print(f'{"Extension":<20} {"AVAILABLE":<12} {"LOCAL":<10} {"GLOBAL":<10}')
    print(f'{"-"*19} {"-"*11} {"-"*9} {"-"*9}')
    for name in all_extension_names:
        available = available_extensions.get(name, '-')
        local = local_extensions.get(name, '-')
        glob = global_extensions.get(name, '-')
        print(f'{name:<20} {available:<12} {local:<10} {glob:<10}')


def add_extension(extension_name: str, source_extensions_dir: Path,
                  target_extensions_dir: Path, symlink: bool) -> None:
    """Adds an extension."""
    source_dir = source_extensions_dir / extension_name
    dest_dir = target_extensions_dir / extension_name

    if dest_dir.exists():
        if not is_up_to_date(extension_name, source_extensions_dir,
                             target_extensions_dir):
            response = input(
                f"Extension '{extension_name}' is already installed but out "
                "of date. Update it? [Y/n] ")
            if response.lower() == 'n':
                return
        else:
            print(f"Extension '{extension_name}' is already installed and up "
                  "to date.")
            return

    if dest_dir.exists():
        _do_remove(dest_dir)

    if symlink:
        os.symlink(source_dir, dest_dir)
    else:
        shutil.copytree(
            source_dir,
            dest_dir,
            ignore=shutil.ignore_patterns('tests'))
    print(f"Added/updated '{extension_name}' to {dest_dir}")


def update_extension(extension_name: str, source_extensions_dir: Path,
                     target_extensions_dir: Path) -> None:
    """Updates an existing extension."""
    source_dir = source_extensions_dir / extension_name
    dest_dir = target_extensions_dir / extension_name

    if not dest_dir.exists():
        print(
            f"Extension '{extension_name}' is not installed in the specified "
            "location. Use 'add' to install it.",
            file=sys.stderr)
        return

    if is_up_to_date(extension_name, source_extensions_dir,
                     target_extensions_dir):
        print(f"Extension '{extension_name}' is already up to date.")
        return

    if dest_dir.exists():
        _do_remove(dest_dir)

    shutil.copytree(source_dir,
                    dest_dir,
                    ignore=shutil.ignore_patterns('tests'))
    print(f"Updated '{extension_name}' in {dest_dir}")


def _do_remove(dest_dir: Path) -> None:
    if dest_dir.is_symlink():
        dest_dir.unlink()
    else:
        shutil.rmtree(dest_dir)


def remove_extension(extension_name: str, target_extensions_dir: Path) -> None:
    """Removes an extension."""
    dest_dir = target_extensions_dir / extension_name
    if dest_dir.exists():
        _do_remove(dest_dir)
        print(f"Removed '{extension_name}' from {target_extensions_dir}")
    else:
        print(
            f"Extension '{extension_name}' not found in "
            f"{target_extensions_dir}",
            file=sys.stderr)


def main() -> None:
    """Installs and manages extension."""
    project_root = get_project_root()
    extensions_dirs = get_extensions_dirs(project_root)

    parser = argparse.ArgumentParser(
        description='Install and manage extensions.')
    subparsers = parser.add_subparsers(
        dest='command',
        help='Available commands.',
        description='Install and manage extensions.'
        ' To get help for a specific command, run "install.py <command> -h".')

    # Add command
    add_parser = subparsers.add_parser('add', help='Add new extension.')
    add_parser.add_argument('-g',
                            '--global',
                            dest='use_global',
                            action='store_true',
                            help='Install to the global extensions directory.')
    add_parser.add_argument(
        '-l',
        '--symlink',
        action='store_true',
        help='Use symlinks rather than directory copies so '
        'that extensions automatically stays up-to-date.')
    add_parser.add_argument('extensions',
                            nargs='+',
                            help='A list of extension directory names to add.')

    # Update command
    update_parser = subparsers.add_parser('update', help='Update extensions.')
    update_parser.add_argument(
        '-g',
        '--global',
        dest='use_global',
        action='store_true',
        help='Update in the global extensions directory.')
    update_parser.add_argument(
        'extensions',
        nargs='*',
        help='A list of extension directory names to update. If not specified, '
        'all installed extensions will be updated.')

    # Remove command
    remove_parser = subparsers.add_parser('remove', help='Remove extensions.')
    remove_parser.add_argument(
        '-g',
        '--global',
        dest='use_global',
        action='store_true',
        help='Remove from the global extensions directory.')
    remove_parser.add_argument(
        'extensions',
        nargs='+',
        help='A list of extension directory names to remove.')

    # List command
    subparsers.add_parser('list',
                          help='List all available and installed extensions.')

    args = parser.parse_args()

    if args.command in ('add', 'update', 'remove'):
        target_extensions_dir = get_extension_dir(project_root, args.use_global)
        if not target_extensions_dir:
            print(
                'Error: Could not determine target directory for local '
                'extensions. Please run from within a gclient project.',
                file=sys.stderr)
            sys.exit(1)

        if args.command in ('add', 'update'):
            target_extensions_dir.mkdir(parents=True, exist_ok=True)

        extensions_to_process = args.extensions
        if args.command == 'update' and not extensions_to_process:
            extensions_to_process = get_installed_extensions(
                target_extensions_dir)

        for extension in extensions_to_process:
            source_extensions_dir = find_extensions_dir_for_extension(
                extension, extensions_dirs)
            if not source_extensions_dir:
                print(f"Error: Extension '{extension}' not found. Skipping.",
                      file=sys.stderr)
                continue

            if args.command == 'add':
                add_extension(extension, source_extensions_dir,
                              target_extensions_dir, args.symlink)
            elif args.command == 'update':
                update_extension(extension, source_extensions_dir,
                                 target_extensions_dir)
            elif args.command == 'remove':
                remove_extension(extension, target_extensions_dir)

    elif args.command == 'list':
        list_extensions(project_root, extensions_dirs)
    else:
        parser.print_help()
        sys.exit(1)


if __name__ == '__main__':
    main()
