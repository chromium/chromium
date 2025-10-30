# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Applying Gemini CLI to Fix Chromium Unsafe Buffer Usage

This is a script to discover and generate spanification fixes for a given file.
"""

import argparse
import contextlib
import json
import os
import re
import subprocess
import sys
import threading

GEMINI_MD_PATH = 'GEMINI.md'  # Assuming the script is run from src
SCRIPT_DIR = os.path.dirname(__file__)

# Prompt:
# This single prompt contains all logic for fixing.
FIX_PROMPT_MD = os.path.join(SCRIPT_DIR, 'prompt.md')
SPANIFICATION_GEMINI_MD = 'SPANIFICATION_GEMINI_MD'

# `gemini-cli` expected outputs:
GEMINI_OUT_DIR = 'gemini_out'
COMMIT_MESSAGE_PATH = GEMINI_OUT_DIR + '/commit_message.md'
SUMMARY_PATH = GEMINI_OUT_DIR + '/summary.json'

# Make sure patches are built and tests on all platforms.
REQUIRED_BUILD_DIRS = [
    'linux-rel',
    'mac-rel',
    'linux-win-cross-rel',
    'android-14-x64-rel',
    'linux-chromeos-rel',
]

# Tools allowed for all tasks:
ALLOWED_TOOLS = [
    # Basic:
    "read_file",
    "replace",
    "write_file",
    "run_shell_command(fdfind)",
    "run_shell_command(rg)",

    # Build/Test
    "run_shell_command(autoninja)",
    "run_shell_command(tools/autotest.py)",
    "run_shell_command(./tools/autotest.py)",

    # Investigate:
    "remote_code_search",
    "run_debugging_agent"
    "run_shell_command(cat)",
    "run_shell_command(git diff)",
    "run_shell_command(git log)",
    "run_shell_command(git show)",
    "run_shell_command(head)",
    "run_shell_command(ls)",
    "run_shell_command(tail)",

    # Cleanup:
    "run_shell_command(git cl format)",

    # Reporting:
    "run_shell_command(touch gemini_out/commit_message.md)",
    "run_shell_command(touch gemini_out/summary.json)",
    "run_shell_command(mkdir -p gemini_out)",
]

def ensure_gn_build_dir():
    """Ensure that the required GN build directories exist."""
    for build_dir in REQUIRED_BUILD_DIRS:
        print(f"Checking for GN build directory 'out/{build_dir}'...")
        if os.path.exists(os.path.join('out', f'{build_dir}')):
            continue
        print(f"GN build directory 'out/{build_dir}' not found. Create one")
        # Run the compile command to create the build directory.
        compile_cmd = [
            'vpython3', 'tools/utr', '-f', '-B', 'try', '-b', build_dir,
            '--build-dir', 'out/' + build_dir, 'compile'
        ]
        result = subprocess.run(compile_cmd, check=False)
        if result.returncode != 0:
            print(f"Error: Failed to create GN build directory "
                  f"'out/{build_dir}'. Exiting.")
            sys.exit(1)


def ensure_docs():
    # Copy ../../../../docs/unsafe_buffers.md to the script directory.
    src_docs_path = os.path.abspath(
        os.path.join(SCRIPT_DIR, '../../../../docs/unsafe_buffers.md'))
    dest_docs_path = os.path.join(SCRIPT_DIR, 'unsafe_buffers.md')
    if not os.path.exists(dest_docs_path):
        subprocess.run(['cp', src_docs_path, dest_docs_path], check=True)

@contextlib.contextmanager
def setup_gemini_context_md(file_path):
    """Context manager to temporarily modify GEMINI.md to include the given
    prompt file."""

    def modify_gemini_md(action, new_entry=""):
        content = ""
        if os.path.exists(GEMINI_MD_PATH):
            with open(GEMINI_MD_PATH, 'r', encoding='utf-8') as f:
                content = f.read()
        else:
            print("Error: the script is expected to be run from the src/ "
                  "directory where GEMINI.md is located.")
            sys.exit(1)

        # Use regex to remove the block between the start and end markers.
        # re.DOTALL allows '.' to match newlines.
        pattern = re.compile(
            f"# {SPANIFICATION_GEMINI_MD}.*?"
            f"# /{SPANIFICATION_GEMINI_MD}\n", re.DOTALL)
        cleaned_content = pattern.sub("", content)

        final_content = cleaned_content
        if action == 'add':
            final_content += new_entry

        with open(GEMINI_MD_PATH, 'w', encoding='utf-8') as f:
            f.write(final_content)

    entry = (f"# {SPANIFICATION_GEMINI_MD}\n"
             f"@{file_path}\n# /{SPANIFICATION_GEMINI_MD}\n")
    modify_gemini_md('add', entry)
    try:
        yield
    finally:
        modify_gemini_md('remove')


def stream_reader(process, output):
    """
    Reads from process.stdout line by line and populates the list.
    This blocks *only* this thread, not the main thread.
    """
    try:
        for line in iter(process.stdout.readline, ''):
            if not line.strip():  # Skip empty lines
                continue

            # Print and process the line immediately
            try:
                json_obj = json.loads(line)
                output.append(json_obj)
                print(json.dumps(json_obj, indent=2))

            except json.JSONDecodeError:
                # This will also print stderr lines since we merged them
                print(line, end='')
    except (IOError, ValueError):
        # This can happen if the pipe is closed abruptly by process.kill()
        pass


def run_gemini(file):
    """
    Run the gemini CLI against the given file to fix unsafe buffer usage.
    Returns the parsed summary.json content.
    """
    prompt = f"Fix the unsafe buffer usage in {file}."

    # Delete `gemini_out` directory if it exists, and recreate it. This is where
    # gemini was instructed to write its outputs.
    if os.path.exists(GEMINI_OUT_DIR):
        subprocess.run(['rm', '-rf', GEMINI_OUT_DIR], check=True)
    os.makedirs(GEMINI_OUT_DIR, exist_ok=True)

    cmd = ['gemini']

    # Headless mode. This allows visualizing the tools used and their outputs
    # while running it.
    cmd.extend(['--output-format', 'stream-json'])

    cmd.extend(['--approval-mode', 'auto_edit'])
    cmd.extend(['--allowed-tools', ','.join(ALLOWED_TOOLS)])

    exit_code = 0
    TIMEOUT_SECONDS = 2700  # 45 minutes
    output = []
    try:
        with subprocess.Popen(
                cmd,
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,  # Merge stdout and stderr
                text=True,
                encoding='utf-8',
                bufsize=1  # Use line-buffering
        ) as process:
            # stdout
            reader_thread = threading.Thread(target=stream_reader,
                                             args=(process, output))
            reader_thread.daemon = True
            reader_thread.start()

            # stdin
            process.stdin.write(prompt)
            process.stdin.close()

            # Wait for the process to complete or timeout
            exit_code = process.wait(timeout=TIMEOUT_SECONDS)

    except subprocess.TimeoutExpired:
        print(f"Error: Process timed out after {TIMEOUT_SECONDS} seconds.",
              file=sys.stderr)
        process.kill()  # Forcefully terminate the process
        exit_code = 124  # Standard timeout exit code

    except Exception as e:
        print(f"An error occurred: {e}", file=sys.stderr)
        if 'process' in locals() and process.poll() is None:
            process.kill()  # Ensure process is dead
        exit_code = 1  # General error

    finally:
        if 'reader_thread' in locals() and reader_thread.is_alive():
            reader_thread.join(timeout=5.0)

    # You can now check the exit_code or inspect the 'output' list
    print(f"\nProcess finished with exit code: {exit_code}")

    exit_code_to_status = {0: 'SUCCESS', 1: 'FAILURE', 124: 'TIMEOUT'}

    # Read the summary.json file generated by gemini.
    summary = {}
    try:
        with open(SUMMARY_PATH, 'r', encoding='utf-8') as f:
            summary = json.load(f)
    except (FileNotFoundError, json.JSONDecodeError):
        print(f"Warning: Could not read {SUMMARY_PATH}.")

    diff_result = subprocess.run(['git', 'diff', file],
                                 capture_output=True,
                                 text=True,
                                 check=False)

    # The commit message is written by gemini to a file.
    commit_message = None
    if os.path.exists(COMMIT_MESSAGE_PATH):
        with open(COMMIT_MESSAGE_PATH, 'r', encoding='utf-8') as f:
            commit_message = f.read()

    return {
        'commit_message': commit_message,
        'diff': diff_result.stdout,
        'exit_code': exit_code,
        'file': file,
        'output': output,
        'status': exit_code_to_status.get(exit_code, 'GEMINI_FAILURE'),
        'summary': summary,
        'prompt': prompt,
    }


def main():
    parser = argparse.ArgumentParser(
        description='Run Gemini to fix unsafe buffer usage in a file.')
    parser.add_argument('file', type=str, help='Path to the file to process.')
    args = parser.parse_args()
    print(f"Processing {args.file}...")

    ensure_gn_build_dir()
    ensure_docs()

    with setup_gemini_context_md(FIX_PROMPT_MD):
        output = run_gemini(args.file)

    with open('gemini_spanification_output.json', 'w', encoding='utf-8') as f:
        json.dump(output, f, indent=2)

if __name__ == '__main__':
    main()
