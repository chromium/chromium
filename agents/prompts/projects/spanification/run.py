# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Applying Gemini CLI to Fix Chromium Unsafe Buffer Usage

This is a script to discover, categorize and generate spanification fixes for
given file.
"""

import subprocess
import contextlib
import json
import os
import sys
import argparse
import re

GEMINI_MD_PATH = 'GEMINI.md'  # Assuming the script is run from src
SCRIPT_DIR = os.path.dirname(__file__)

# Prompts:
CATEGORIZE_PROMPT_MD = os.path.join(SCRIPT_DIR, 'prompt_categorize.md')
FIXING_PROMPT_MD = os.path.join(SCRIPT_DIR, 'prompt_fixing.md')
SPANIFICATION_GEMINI_MD = 'SPANIFICATION_GEMINI_MD'

# `gemini-cli` expected outputs:
GEMINI_OUT_DIR = 'gemini_out'
COMMIT_MESSAGE_PATH = GEMINI_OUT_DIR + '/commit_message.md'
SUMMARY_PATH = GEMINI_OUT_DIR + '/summary.json'


def ensure_gn_build_dir():
    """Ensure that the required GN build directories exist."""
    REQUIRED_BUILD_DIRS = [
        'linux-rel',
        'linux-win-cross-rel',
        'android-14-x64-rel',
    ]
    for build_dir in REQUIRED_BUILD_DIRS:
        print(f"Checking for GN build directory 'out/UTR{build_dir}'...")
        if os.path.exists(os.path.join('out', f'UTR{build_dir}')):
            continue
        print(f"GN build directory 'out/UTR{build_dir}' not found. Create one")
        # Run the compile command to create the build directory.
        compile_cmd = [
            'vpython3', 'tools/utr', '-f', '-B', 'try', '-b', build_dir,
            'compile'
        ]
        result = subprocess.run(compile_cmd, check=False)
        if result.returncode != 0:
            print(f"Error: Failed to create GN build directory "
                  f"'out/UTR{build_dir}'. Exiting.")
            sys.exit(1)

def discover_unsafe_todos(folder=None):
    cmd = [
        'git', 'grep', '-l', '-e', 'UNSAFE_TODO', '--or', '-e',
        'allow_unsafe_buffers'
    ]
    if folder:
        cmd.append(folder)

    result = subprocess.run(cmd, capture_output=True, text=True, check=False)
    if result.returncode != 0:
        # git grep returns 1 if no lines are selected, which is not an error.
        if result.returncode == 1 and result.stdout == "":
            return []
        print("Error discovering files:", result.stderr)
        return []
    return result.stdout.strip().split('\n')


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


def run_gemini(prompt, task_args):
    """
    Run the gemini CLI with the given prompt and task arguments.
    Returns the parsed summary.json content.
    """

    # Ensure the directory for gemini outputs exists. This is important, because
    # gemini is not allowed to create directories, and we ask it to write
    # outputs there.
    if not os.path.exists('gemini_out'):
        os.makedirs('gemini_out')
    # Clean up previous run files
    for f in [SUMMARY_PATH, COMMIT_MESSAGE_PATH]:
        if os.path.exists(f):
            os.remove(f)

    cmd = ['gemini']

    # Headless mode. This allows visualizing the tools used and their outputs
    # while running it.
    cmd.extend(['--output-format', 'stream-json'])

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
        "run_shell_command(./tools/autotest.py)",

        # Investigate:
        "remote_code_search",
        "run_debugging_agent"
        "run_shell_command(git log)",
        "run_shell_command(git diff)",

        # Cleanup:
        "run_shell_command(git cl format)",
    ]
    cmd.extend(['--approval-mode', 'auto_edit'])
    cmd.extend(['--allowed-tools', ','.join(ALLOWED_TOOLS)])

    exit_code = 0

    output = []
    with subprocess.Popen(cmd,
                          stdin=subprocess.PIPE,
                          stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT,
                          text=True,
                          encoding='utf-8') as process:
        try:
            # Send the prompt to gemini's stdin.
            process.stdin.write(prompt)
            process.stdin.close()

            # Read gemini's stdout line by line. They are in JSON format.
            for line in iter(process.stdout.readline, ''):
                try:
                    json_obj = json.loads(line)
                    print(json.dumps(json_obj, indent=2))
                    output.append(json_obj)
                except json.JSONDecodeError:
                    print(line, end='')  # Print non-JSON lines as is

            exit_code = process.wait(timeout=3000)

        except subprocess.TimeoutExpired:
            process.kill()
            exit_code = 124  # timeout exit code in linux

    exit_code_to_status = {0: 'SUCCESS', 1: 'FAILURE', 124: 'TIMEOUT'}

    # Read the summary.json file generated by gemini.
    summary = {}
    try:
        with open(SUMMARY_PATH, 'r', encoding='utf-8') as f:
            summary = json.load(f)
    except (FileNotFoundError, json.JSONDecodeError):
        print(f"Warning: Could not read {SUMMARY_PATH}.")

    return {
        'task': {
            'prompt': prompt,
            'args': task_args,
        },
        'status': exit_code_to_status.get(exit_code, 'GEMINI_FAILURE'),
        'exit_code': exit_code,
        'output': output,
        'summary': summary,
    }


def categorize_file(file_path):
    """Categorize the unsafe buffer usage in the given file."""
    with setup_gemini_context_md(CATEGORIZE_PROMPT_MD):
        prompt = (
            f"detect the unsafe access and variable category for {file_path}")
        return run_gemini(prompt, task_args=[file_path])


def generate_fix(file_path, variable_type=None, access_type=None):
    """Generate spanification fix for the given file based on its categories."""

    VARIABLE_PROMPTS = {
        'Already-Safe': 'No changes to the variable are needed.',
        'Local-Variable': 'Arrayify the variable using `std::to_array`.',
        'Local-Method-Argument': (
            'Change the method signature to take a `std::span`.'
        ),
        'Class-Method-with-Safe-Variant': (
            'Replace the unsafe methods (that return a buffer) '
            'with a safe variant.'
        ),
        'Method-Argument': (
            'Change the method signature to take a `std::span` '
            'and update all call sites.'
        ),
        'Global-Variable': (
            'Arrayify the variable using `std::to_array` '
            'and update all usages.'
        ),
        'Class-Method-Safe-Variant-TODO': (
            'Migrate the internal members to safe containers '
            'and create a new safe method variant.'
        ),
    }

    ACCESS_PROMPTS = {
        'operator[]': (
            'The access should be safe now, '
            'just remove the `UNSAFE_TODO`.'
        ),
        'Pointer-Arithmetic': (
            'Use `base::span::first(N)`, `base::span::subspan(offset, count)` '
            '... instead.'
        ),
        'Safe-Container-Construction': (
            'Convert `base::span(pointer, size)` to a safe constructor '
            'like `base::span(container)`. If the size changed, you could use '
            '`base::span(other_span).subspan(...)` or `first(...)` to create '
            'safe views into existing spans.'
        ),
        'std::memcmp': 'Replace the comparison with `operator==`.',
        'std::strcmp': 'Replace the comparison with `operator==`.',
        'std::memcpy': 'Replace the copy with `base::span::copy_from()`.',
        'std::strncpy': 'Replace the copy with `base::span::copy_from()`.',
        'std::strcpy': 'Replace the copy with `base::span::copy_from()`.',
        'std::memset': (
            'Replace memset with `std::ranges::fill()` or `<instance> = {}`.'
        ),
        'std::strstr': 'Replace the search with `std::string_view::find()`.',
        'std::wcslen': 'Just get size() from the safe container.',
        'std::strlen': 'Just get size() from the safe container.',
    }

    variable_prompt = VARIABLE_PROMPTS.get(variable_type, '')
    access_prompt = ACCESS_PROMPTS.get(access_type, '')
    task_args = [file_path, variable_type, access_type]

    if not variable_prompt and not access_prompt:
        generated_prompt = f"Fix the unsafe buffer usage in {file_path}."
    elif not variable_prompt:
        print(f"Warning: Unknown variable_type ('{variable_type}').")
        return {
            'status': 'NOT_SUPPORTED',
            'summary': f"Unknown variable_type: {variable_type}",
            'task_args': task_args,
            'duration': 0,
        }
    elif not access_prompt:
        print(f"Warning: Unknown access_type ('{access_type}').")
        return {
            'status': 'NOT_SUPPORTED',
            'summary': f"Unknown access_type: {access_type}",
            'task_args': task_args,
            'duration': 0,
        }
    else:
        generated_prompt = (
            f"The variable in {file_path} is of type {variable_type}. "
            f"{variable_prompt} The unsafe access pattern is {access_type}. "
            f"{access_prompt} ")

    with setup_gemini_context_md(FIXING_PROMPT_MD):
        return run_gemini(generated_prompt, task_args)


def autocommit_changes(fix_result, file_path):
    """Automatically commit changes if the fix was successful,
    otherwise reset."""
    is_success = fix_result.get('status') == 'SUCCESS'

    if is_success:
        print(f"Successfully fixed {file_path}. Committing changes.")
        if os.path.exists(COMMIT_MESSAGE_PATH):
            subprocess.run(['git', 'commit', '-a', '-F', COMMIT_MESSAGE_PATH],
                           check=True)
            print(f"Committed fix for {file_path}.")
        else:
            print(f"Warning: {COMMIT_MESSAGE_PATH} not found. Cannot commit. "
                  "Resetting to HEAD.")
            subprocess.run(['git', 'reset', '--hard', 'HEAD'], check=True)
    else:
        print(f"Fix generation failed for {file_path}. Resetting to HEAD.")
        subprocess.run(['git', 'reset', '--hard', 'HEAD'], check=True)


def main():
    parser = argparse.ArgumentParser(
        description='Discover, categorize and generate spanification fixes.')
    parser.add_argument('path',
                        nargs='?',
                        default=None,
                        help='The file or folder to process.')
    parser.add_argument('--categorize-only',
                        action='store_true',
                        help='Only run the categorization step.')
    parser.add_argument('--fix-only',
                        action='store_true',
                        help='Only run the fix generation step.')
    parser.add_argument('--autocommit',
                        action='store_true',
                        help='Automatically commit successful fixes.')
    args = parser.parse_args()

    ensure_gn_build_dir()

    outputs = []

    files_to_process = []
    if args.path:
        if os.path.isdir(args.path):
            files_to_process = discover_unsafe_todos(args.path)
        else:
            files_to_process.append(args.path)
    else:
        files_to_process = discover_unsafe_todos()

    if not files_to_process:
        print("No files to process.")
        return

    for file_path in files_to_process:
        output = {}

        print("=" * 40)
        print(f"Processing {file_path}...")
        categorization_result = None
        if not args.fix_only:
            print("Categorization".center(40, '-'))
            categorization_result = categorize_file(file_path)
            output['categorization'] = categorization_result

        if not args.categorize_only:
            print("Fix Generation".center(40, '-'))
            variable_type = None
            access_type = None

            if categorization_result:
                if categorization_result.get('status') != 'SUCCESS':
                    print(f"Skipping fix generation for {file_path} due to "
                          "categorization failure.")
                    continue
                # The categorization step is expected to return 'variable_type'
                # and 'access_type' in summary.json
                summary = categorization_result.get('summary', {})
                variable_type = summary.get('variable_type')
                access_type = summary.get('access_type')

            fix_result = generate_fix(file_path, variable_type, access_type)
            output['fix'] = fix_result

            # Retrieve the diff and commit message if available.
            # The diff can be obtained using 'git diff' since gemini writes
            diff_result = subprocess.run(['git', 'diff', file_path],
                                         capture_output=True,
                                         text=True,
                                         check=False)
            output['diff'] = diff_result.stdout

            # The commit message is written by gemini to a file.
            if os.path.exists(COMMIT_MESSAGE_PATH):
                with open(COMMIT_MESSAGE_PATH, 'r', encoding='utf-8') as f:
                    commit_message = f.read()
                output['commit_message'] = commit_message
            else:
                output['commit_message'] = None

            if args.autocommit:
                print("Committing changes".center(40, '-'))
                autocommit_changes(fix_result, file_path)

            outputs.append({file_path: output})

    # Save the outputs to a JSON file for further analysis.
    with open('gemini_spanification_output.json', 'w', encoding='utf-8') as f:
        json.dump(outputs, f, indent=2)


if __name__ == '__main__':
    main()
