# Writing GN Templates
GN and Ninja are documented here:
* GN: https://gn.googlesource.com/gn/+/master/docs/
* Ninja: https://ninja-build.org/manual.html

[TOC]

## Things to Consider When Writing Templates
### Inputs and Depfiles
List all files read (or executed) by an action as `inputs`.
 * It is not enough to have inputs listed by dependent targets. They must be
   listed directly by targets that use them, or added by a depfile.
 * Non-system Python imports are inputs! For scripts that import such modules,
   use [`action_with_pydeps`] to ensure all dependent Python files are captured
   as inputs.

[`action_with_pydeps`]: https://cs.chromium.org/chromium/src/build/config/python.gni?rcl=320ee4295eb7fabaa112f08d1aacc88efd1444e5&l=75

To understand *why* actions must list all inputs directly, you need to
understand ninja's "restat" directive, which is used for all GN `action()`s.

From https://ninja-build.org/manual.html:

> if present, causes Ninja to re-stat the command’s outputs after execution of
> the command. Each output whose modification time the command did not change
> will be treated as though it had never needed to be built. This may cause the
> output’s reverse dependencies to be removed from the list of pending build
> actions.

So, if your action depends on target "X", and "X" does not change its outputs
when rebuilt, then ninja will not bother to rebuild your target.

For action inputs that are not computable during "gn gen", actions can write
depfiles (.d files) to add additional input files as dependencies for
subsequent builds. They are relevant only for incremental builds since they
won't exist for the initial build.
 * Depfiles should not list files that GN already lists as `inputs`.
   * Besides being redundant, listing them also makes it harder to remove
     inputs, since removing them from GN does not immediately remove them from
     depfiles.
   * Stale paths in depfiles can cause ninja to complain of circular
     dependencies [in some cases](https://bugs.chromium.org/p/chromium/issues/detail?id=639042).

### Ensuring "gn analyze" Knows About your Inputs
"gn analyze" is used by bots to run only affected tests and build only affected
targets. Try it out locally via:
```bash
echo "compute_inputs_for_analyze = true" >> out/Debug/args.gn
gn analyze //out/Debug <(echo '{
    "files": ["//BUILD.gn"],
    "test_targets": ["//base"],
    "additional_compile_targets":[]}') result.txt; cat result.txt
```
* For analyze to work properly, GN must know about all inputs.
* Inputs added by depfiles are *not available* to "gn analyze".
  * When paths listed in a target's depfile are listed as `inputs` to a
    dependent target, analyze will be correct.
    * Example: An  `AndroidManifest.xml` file is an input to an
      `android_library()` and is included in an `android_apk()`'s depfile.
      `gn analyze` will know that a change to the file will require the APK
      to be rebuilt, because the file is marked as an input to the library, and
      the library is a dep of the APK.
  * When paths listed in a target's depfile are *not* listed as `inputs` to a
    dependent target, a few options exist:
    * Rather than putting the inputs in a depfile, force users of your template
      to list them, and then have your action re-compute them and assert that
      they were correct.
      * `jinja_template()` does this.
    * Rather than putting the inputs in a depfile, compute them beforehand and
      save them to a text file. Have your template Use `read_file()` to read
      them in.
      * `action_with_pydeps()` does this.
    * Continue using a depfile, but use an `exec_script()` to compute them when
      [`compute_inputs_for_analyze`](https://cs.chromium.org/chromium/src/build/config/compute_inputs_for_analyze.gni)
      is set.
      * `grit()` does this.

### Outputs
#### What to List as Outputs
Do not list files as `outputs` unless they are important. Outputs are important
if they are:
  * used as an input by another target, or
  * are roots in the dependency graph (e.g. binaries, apks, etc).

Example:
* An action runs a binary that creates an output as well as a log file. Do not
  list the log file as an output.

Rationale:
* Inputs and outputs are a node's public API on the build graph. Not listing
  "implementation detail"-style outputs prevents other targets from depending on
  them as inputs.
* Not listing them also helps to minimize the size of the build graph (although
  this would be noticeable only for frequently used templates).

#### Where to Place Outputs
**Option 1:** To make outputs visible in codesearch (e.g. generated sources):
* use `$target_gen_dir/$target_name.$EXTENSION`.

**Option 2:** Otherwise (for binary files):
* use `$target_out_dir/$target_name.$EXTENSION`.

**Option 3:** For outputs that are required at runtime
(e.g. [runtime_deps](https://gn.googlesource.com/gn/+/master/docs/reference.md#runtime_deps)),
options 1 & 2 do not work because they are not archived in builder/tester bot
configurations. In this case:
* use `$root_out_dir/gen.runtime` or `$root_out_dir/obj.runtime`.

Example:
```python
# This .json file is used at runtime and thus cannot go in target_gen_dir.
_target_dir_name = rebase_path(get_label_info(":$target_name", "dir"), "//")
_output_path = "$root_out_dir/gen.runtime/$_target_dir_name/$target_name.json"
```

**Option 4:** For outputs that map 1:1 with executables, and whose paths cannot
be derived at runtime:
* use `$root_build_dir/YOUR_NAME_HERE/$target_name`.

Examples:
```python
# Wrapper scripts for apks:
_output_path = "$root_build_dir/bin/$target_name"
# Metadata for apks. Used by binary size tools.
_output_path = "$root_build_dir/size-info/${invoker.name}.apk.jar.info"
```

## Best Practices for Python Actions
Outputs should be atomic and take advantage of `restat=1`.
* Make outputs atomic by writing to temporary files and then moving them to
  their final location.
  * Rationale: An interrupted write can leave a file with an updated timestamp
    and corrupt contents. Ninja looks only at timestamps.
* Do not overwrite an existing output with identical contents.
  * Rationale: `restat=1` is a ninja feature enabled for all actions that
    short-circuits a build when output timestamps do not change. This feature is
    the reason that the total number of build steps sometimes decreases when
    building..
* Use [`build_utils.AtomicOutput()`](https://cs.chromium.org/chromium/src/build/android/gyp/util/build_utils.py?rcl=7d6ba28e92bec865a7b7876c35b4621d56fb37d8&l=128)
  to perform both of these techniques.

Actions should be deterministic in order to avoid hard-to-reproduce bugs.
Given identical inputs, they should produce byte-for-byte identical outputs.
* Some common mistakes:
  * Depending on filesystem iteration order.
  * Writing timestamps in files (or in zip entries).
  * Writing absolute paths in outputs.

## Style Guide
Chromium GN files follow
[GN's Style Guide](https://gn.googlesource.com/gn/+/master/docs/style_guide.md)
with a few additions.

### Action Granularity
 * Prefer writing new Python scripts that do what you want over
   composing multiple separate actions within a template.
   * Fewer targets makes for a simpler build graph.
   * GN logic and build logic winds up much simpler.

Bad:
```python
template("generate_zipped_sources") {
  generate_files("${target_name}__gen") {
    ...
    outputs = [ "$target_gen_dir/$target_name.temp" ]
  }
  zip(target_name) {
    deps = [ ":${target_name}__gen" ]
    inputs = [ "$target_gen_dir/$target_name.temp" ]
    outputs = [ invoker.output_zip ]
  }
}
```

Good:
```python
template("generate_zipped_sources") {
  action(target_name) {
    script = "generate_and_zip.py"
    ...
    outputs = [ invoker.output_zip ]
  }
}
```

### Naming for Intermediate Targets
Targets that are not relevant to users of your template should be named as:
`${target_name}__$something`.

Example:
```python
template("my_template") {
  action("${target_name}__helper") {
    ...
  }
  action(target_name) {
    deps = [ ":${target_name}__helper" ]
    ...
  }
}
```

This scheme ensures that subtargets defined in templates do not conflict with
top-level targets.

### Visibility for Intermediate Targets

You can restrict what targets can depend on one another using [visibility].
When writing templates, with multiple intermediate targets, `visibility` should
only be applied to the final target (the one named `target_name`). Applying only
to the final target ensures that the invoker-provided visibility does not
prevent intermediate targets from depending on each other.

[visibility]: https://gn.googlesource.com/gn/+/master/docs/reference.md#var_visibility

Example:
```python
template("my_template") {
  # Do not forward visibility here.
  action("${target_name}__helper") {
    # Do not forward visibility here.
    ...
  }
  action(target_name) {
    # Forward visibility here.
    forward_variables_from(invoker, [ "visibility" ])
    deps = [ ":${target_name}__helper" ]
    ...
  }
}
```

### Variables
Prefix variables within templates and targets with an underscore. For example:

```python
template("example") {
  _outer_sources = invoker.extra_sources

  source_set(target_name) {
    _inner_sources = invoker.sources
    sources = _outer_sources + _inner_sources
  }
}
```

This convention conveys that `sources` is relevant to `source_set`, while
`_outer_sources`  and `_inner_sources` are not.

### Passing Arguments to Targets
Pass arguments to targets by assigning them directly within target definitions.

When a GN template goes to resolve `invoker.FOO`, GN will look in all enclosing
scopes of the target's definition. It is hard to figure out where `invoker.FOO`
is coming from when it is not assigned directly within the target definition.

Bad:
```python
template("hello") {
  script = "..."
  action(target_name) {
    # This action will see "script" from the enclosing scope.
  }
}
```

Good:
```python
template("hello") {
  action(target_name) {
    script = "..."  # This is equivalent, but much more clear.
  }
}
```

**Exception:** `testonly` and `visibility` can be set in the outer scope so that
they are implicitly passed to all targets within a template.

This is okay:
```python
template("hello") {
  testonly = true  # Applies to all nested targets.
  action(target_name) {
    script = "..."
  }
}
```

### Using forward_variables_from()
Using [forward_variables_from()] is encouraged, but special care needs to be
taken when forwarding `"*"`. The variables `testonly` and `visibility` should
always be listed explicitly in case they are assigned in an enclosing
scope.
See [this bug] for more a full example.

To make this easier, `//build/config/BUILDCONFIG.gn` defines:
```python
TESTONLY_AND_VISIBILITY = [ "testonly", "visibility" ]
```

Example usage:
```python
template("action_wrapper") {
  action(target_name) {
    forward_variables_from(invoker, "*", TESTONLY_AND_VISIBILITY)
    forward_variables_from(invoker, TESTONLY_AND_VISIBILITY)
    ...
  }
}
```

If your template defines multiple targets, be careful to apply `testonly` to
both, but `visibility` only to the primary one (so that the primary one is not
prevented from depending on the other ones).

Example:
```python
template("template_with_multiple_targets") {
  action("${target_name}__helper) {
    forward_variables_from(invoker, [ "testonly" ])
    ...
  }
  action(target_name) {
    forward_variables_from(invoker, TESTONLY_AND_VISIBILITY)
    ...
  }
}
```

An alternative would be to explicitly set `visibility` on all inner targets,
but doing so tends to be tedious and has little benefit.

[this bug]: https://bugs.chromium.org/p/chromium/issues/detail?id=862232
[forward_variables_from]: https://gn.googlesource.com/gn/+/master/docs/reference.md#func_forward_variables_from

## Useful Ninja Flags
Useful ninja flags when developing build rules:
* `ninja -v` - log the full command-line of every target.
* `ninja -v -n` - log the full command-line of every target without having
  to wait for a build.
* `ninja -w dupbuild=err` - fail if multiple targets have the same output.
* `ninja -d keeprsp` - prevent ninja from deleting response files.
* `ninja -n -d explain` - print why ninja thinks a target is dirty.
* `ninja -j1` - execute only one command at a time.
