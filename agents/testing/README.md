# Prompt Evaluation

This directory contains the framework for running prompt evaluation
tests on the choromium code base using extensions and prompts under `//agents`.

Googlers please see also go/chromium-prompt-evaluations.

## Usage

Existing tests can be run via the `//agents/testing/eval_prompts.py` script. It
should handle everything automatically, although it is advised to commit any
changes before running this script. It will automatically retrieve a temporary
copy of promptfoo, perform repo setup, run configured tests, and perform
teardown.

By default, it will build promptfoo from ToT, but specific behavior can be
configured via command line arguments, including use of stable releases via npm
which will likely result in faster setup.

### Filtering by tags

Tests can be filtered by tags by passing the `--tag-filter` argument,
followed by a comma-separated list of tags. Only tests that have at least one
of the given tags will be run. Tags can be added to tests by adding a `tags`
field to the test's metadata in its `promptfoo.yaml` file.

```yaml
tests:
  - metadata:
    tags: ['my-tag']
```

### Running without a container runtime

If you are running `eval_prompts.py` on a system without a container runtime
like Docker or Podman, you will need to pass the `--no-sandbox` flag. This
is because the script uses sandboxing by default to isolate the test
environment.

### btrfs Chromium Setup (Strongly recommended!)

The prompt eval is intended to be run with Chromium in a btrfs file system.
The tests should still run in a normal checkout but will be significantly
slower and take up significantly more disk space. These steps can be used to
fetch a new Chromium solution in a virtual btrfs file system mounted in your
home dir.

The following commands can be used to set up the environment:
```bash
# Ensure btrfs is installed
sudo apt install btrfs-progs

# Create the virtual image file
truncate -s 500G ~/btrfs_virtual_disk.img

# Format the image with btrfs
mkfs.btrfs ~/btrfs_virtual_disk.img

# Mount the image
mkdir ~/btrfs
sudo mount -o loop ~/btrfs_virtual_disk.img ~/btrfs

# Update owner
sudo chown $(whoami):$(id -ng) ~/btrfs

# Create a btrfs subvolume for the checkout
btrfs subvolume create ~/btrfs/chromium

# Fetch a new Chromium checkout into the subvolume.
# This will place the 'src' directory inside '~/btrfs/chromium/'.
cd ~/btrfs/chromium
fetch chromium

# For an existing checkout, you would instead move the contents, e.g.:
# mv ~/your_old_chromium/* ~/btrfs/chromium/

# (Optional) To make the mount permanent, add it to /etc/fstab.
# It's wise to back up this critical file first.
cp /etc/fstab ~/fstab.bak
echo "$HOME/btrfs_virtual_disk.img $HOME/btrfs btrfs loop,defaults 0 0" | sudo tee -a /etc/fstab
```

After Chromium is checked out, `agents/testing/eval_prompts.py` can then
be run from `~/btrfs/chromium/src/`.

This checkout should function just like your original so you don't need to
maintain both if you prefer.

## Adding Tests

Each independent test case should have its own promptfoo yaml config file. See
the [promptfoo
documentation](https://www.promptfoo.dev/docs/configuration/guide/) for more
information on this. If multiple prompts are expected to result in the same
behavior, and thus can be tested in the same way, the config file can contain
multiple prompts. promptfoo will automatically test each prompt individually.

Config files should be placed in a subdirectory of the
relevant prompt or extension directory. The tests will be discovered by the
test runner and ran based on any filter or sharding args passed to the runner.

## Advanced Usage: Testing Custom Options

The `gemini_provider.py` supports several custom options for advanced testing
scenarios, such as applying file changes or loading specific templates. Below is
an example of a `promptfoo.yaml` file that demonstrates how to use the `changes`
option to patch and stage files before a test prompt is run.

This example can be used as a template for writing tests that require a specific
file state.

### Example: `test_with_custom_options.promptfoo.yaml`

```yaml
prompts:
  - "What is the staged content of the file `path/to/dummy.txt`?"
providers:
  - id: "python:../../../testing/gemini_provider.py"
    config:
      extensions:
        - depot_tools
      changes:
        - apply: "path/to/add_dummy_content.patch"
        - stage: "path/to/dummy.txt"
tests:
  - description: "Test with custom options"
    assert:
      # Check that the agent ran git diff and found the new content.
      - type: icontains
        value: "dummy content"
    metadata:
      # The compile targets that should be compiled before the prompt runs
      precompile_targets:
      - foo_unittests
      runs_per_test: 10 # The number of iterations to run
      pass_k_threshold: 5 # The number of iterations that must pass
      tags: ['my-tag']
```

### Example Patch File

The `changes` field points to standard `.patch` files. The test runner will
apply them.

#### `add_dummy_content.patch`
```diff
diff --git a/path/to/dummy.txt b/path/to/dummy.txt
index e69de29..27332d3 100644
--- a/path/to/dummy.txt
+++ b/path/to/dummy.txt
@@ -0,0 +1 @@
+dummy content

```