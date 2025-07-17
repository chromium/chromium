#!/usr/bin/env vpython3

# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Installs and manages configurations for MCP servers for the Gemini CLI.

This script allows you to install MCP server configurations from the
'agents/mcp' directory into the Gemini CLI extensions directory. These
configurations are used by the Gemini CLI to connect to and interact with
MCP servers. You can install configurations at the project level (in the
'.gemini/extensions' folder at the root of the git repository) or globally
(in '~/.gemini/extensions').
"""

import argparse
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path


def get_mcp_servers(mcp_dir: Path) -> list[str]:
    """Returns a list of all MCP servers in the given directory.

    Args:
        mcp_dir: The directory containing the MCP server configurations.

    Returns:
        A list of server names.
    """
    return [p.parent.name for p in mcp_dir.glob('*/gemini-extension.json')]


def get_git_repo_root() -> Path | None:
    """Returns the root of the git repository."""
    try:
        return Path(
            subprocess.check_output(['git', 'rev-parse', '--show-toplevel'],
                                    encoding='utf-8').strip())
    except (subprocess.CalledProcessError, FileNotFoundError):
        return None


def get_extension_dir(use_global: bool = False) -> Path:
    """Returns the Gemini CLI extension directory."""
    if use_global:
        return Path.home() / '.gemini' / 'extensions'

    repo_root = get_git_repo_root()
    if repo_root:
        return repo_root / '.gemini' / 'extensions'
    return Path('.gemini/extensions')


def get_installed_servers(extension_dir: Path) -> list[str]:
    """Returns a list of all installed MCP servers.

    Args:
        extension_dir: The extension directory to search.

    Returns:
        A list of installed server names.
    """
    if not extension_dir.exists():
        return []
    return [
        p.parent.name for p in extension_dir.glob('*/gemini-extension.json')
    ]


def get_server_version(server_path: Path) -> str:
    """Returns the version of the server from its manifest file."""
    manifest_path = server_path / 'gemini-extension.json'
    if not manifest_path.exists():
        return '-'
    with open(manifest_path, 'r', encoding='utf-8') as f:
        try:
            data = json.load(f)
            return data.get('version', '-')
        except json.JSONDecodeError:
            return '-'


def get_dir_hash(directory: Path) -> bytes | None:
    """Calculates a hash for the contents of a directory."""
    hashes = []
    for path in sorted(Path(directory).rglob('*')):
        if path.is_file():
            try:
                hashes.append(
                    subprocess.check_output(['git', 'hash-object',
                                             str(path)]).strip())
            except (subprocess.CalledProcessError, FileNotFoundError):
                # Fallback for non-git environments
                import hashlib
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
        import hashlib
        hasher = hashlib.sha1()
        for h in hashes:
            hasher.update(h)
        return hasher.hexdigest().encode('utf-8')


def is_up_to_date(server_name: str, mcp_dir: Path,
                  extension_dir: Path) -> bool:
    """Checks if the installed server configuration is up to date."""
    source_dir = mcp_dir / server_name
    dest_dir = extension_dir / server_name

    if not dest_dir.exists():
        return False

    source_hash = get_dir_hash(source_dir)
    dest_hash = get_dir_hash(dest_dir)

    return source_hash == dest_hash


def list_servers(mcp_dir: Path) -> None:
    """Lists all available and installed MCP servers."""
    # Get available, local, and global servers
    available_servers = {
        name: get_server_version(mcp_dir / name)
        for name in get_mcp_servers(mcp_dir)
    }
    local_servers = {
        name: get_server_version(get_extension_dir(use_global=False) / name)
        for name in get_installed_servers(get_extension_dir(use_global=False))
    }
    global_servers = {
        name: get_server_version(get_extension_dir(use_global=True) / name)
        for name in get_installed_servers(get_extension_dir(use_global=True))
    }

    all_server_names = sorted(
        set(available_servers.keys())
        | set(local_servers.keys())
        | set(global_servers.keys()))

    # Print table
    print(f'{"MCP Server":<20} {"AVAILABLE":<12} {"LOCAL":<10} {"GLOBAL":<10}')
    print(f'{"-"*19} {"-"*11} {"-"*9} {"-"*9}')
    for name in all_server_names:
        available = available_servers.get(name, '-')
        local = local_servers.get(name, '-')
        glob = global_servers.get(name, '-')
        print(f'{name:<20} {available:<12} {local:<10} {glob:<10}')


def add_server(server_name: str, mcp_dir: Path, extension_dir: Path) -> None:
    """Adds a new MCP server configuration."""
    source_dir = mcp_dir / server_name
    dest_dir = extension_dir / server_name

    if dest_dir.exists():
        if not is_up_to_date(server_name, mcp_dir, extension_dir):
            response = input(
                f"Server '{server_name}' is already installed but out of date. "
                "Update it? [Y/n] ")
            if response.lower() == 'n':
                return
        else:
            print(
                f"Server '{server_name}' is already installed and up to date.")
            return

    if dest_dir.exists():
        shutil.rmtree(dest_dir)
    shutil.copytree(source_dir, dest_dir)
    print(f"Added/updated '{server_name}' to {dest_dir}")


def update_server(server_name: str, mcp_dir: Path,
                  extension_dir: Path) -> None:
    """Updates an existing MCP server configuration."""
    source_dir = mcp_dir / server_name
    dest_dir = extension_dir / server_name

    if not dest_dir.exists():
        print(
            f"Server '{server_name}' is not installed in the specified "
            "location. Use 'add' to install it.",
            file=sys.stderr)
        return

    if is_up_to_date(server_name, mcp_dir, extension_dir):
        print(f"Server '{server_name}' is already up to date.")
        return

    if dest_dir.exists():
        shutil.rmtree(dest_dir)
    shutil.copytree(source_dir, dest_dir)
    print(f"Updated '{server_name}' in {dest_dir}")


def remove_server(server_name: str, extension_dir: Path) -> None:
    """Removes an MCP server configuration.

    Args:
        server_name: The name of the server to remove.
        extension_dir: The extension directory to remove the server from.
    """
    dest_dir = extension_dir / server_name
    if dest_dir.exists():
        shutil.rmtree(dest_dir)
        print(f"Removed '{server_name}' from {extension_dir}")
    else:
        print(f"Server '{server_name}' not found in {extension_dir}",
              file=sys.stderr)


def main() -> None:
    """Installs and manages MCP server configurations."""
    mcp_dir = Path(__file__).parent.resolve()

    parser = argparse.ArgumentParser(
        description='Install and manage MCP server configurations.')
    subparsers = parser.add_subparsers(dest='command',
                                       help='Available commands.')

    # Add command
    add_parser = subparsers.add_parser('add', help='Add new MCP servers.')
    add_parser.add_argument('-g',
                            '--global',
                            dest='use_global',
                            action='store_true',
                            help='Install to the global extensions directory.')
    add_parser.add_argument('servers',
                            nargs='+',
                            help='A list of server directory names to add.')

    # Update command
    update_parser = subparsers.add_parser('update', help='Update MCP servers.')
    update_parser.add_argument(
        '-g',
        '--global',
        dest='use_global',
        action='store_true',
        help='Update in the global extensions directory.')
    update_parser.add_argument(
        'servers',
        nargs='*',
        help='A list of server directory names to update. If not specified, '
        'all installed servers will be updated.')

    # Remove command
    remove_parser = subparsers.add_parser('remove', help='Remove MCP servers.')
    remove_parser.add_argument(
        '-g',
        '--global',
        dest='use_global',
        action='store_true',
        help='Remove from the global extensions directory.')
    remove_parser.add_argument(
        'servers',
        nargs='+',
        help='A list of server directory names to remove.')

    # List command
    subparsers.add_parser('list',
                          help='List all available and installed MCP servers.')

    args = parser.parse_args()

    if args.command in ('add', 'update', 'remove'):
        extension_dir = get_extension_dir(args.use_global)
        if args.command in ('add', 'update'):
            extension_dir.mkdir(parents=True, exist_ok=True)

        servers_to_process = args.servers
        if args.command == 'update' and not servers_to_process:
            servers_to_process = get_installed_servers(extension_dir)

        for server in servers_to_process:
            if not (mcp_dir / server).exists():
                print(f"Error: Server '{server}' not found. Skipping.",
                      file=sys.stderr)
                continue

            if args.command == 'add':
                add_server(server, mcp_dir, extension_dir)
            elif args.command == 'update':
                update_server(server, mcp_dir, extension_dir)
            elif args.command == 'remove':
                remove_server(server, extension_dir)

    elif args.command == 'list' or args.command is None:
        list_servers(mcp_dir)


if __name__ == '__main__':
    main()