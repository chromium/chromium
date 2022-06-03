# DEPS Files

DEPS files specify which files the sources in a directory tree may include.

## File format

First you have the normal module-level deps. These are the ones used by
gclient. An example would be:

```
deps = {
  "base":"http://foo.bar/trunk/base"
}
```

DEPS files not in the top-level of a module won't need this. Then you have any
additional include rules. You can add (using `+`) or subtract (using `-`) from
the previously specified rules (including module-level deps). You can also
specify a path that is allowed for now but that we intend to remove, using `!`;
this is treated the same as `+` when `check_deps` is run by our bots, but a
presubmit step will show a warning if you add a new include of a file that is
only allowed by `!`.

Note that for .java files, there is currently no difference between `+` and
`!`, even in the presubmit step.

```
include_rules = [
  # Code should be able to use base (it's specified in the module-level
  # deps above), but nothing in "base/evil" because it's evil.
  "-base/evil",

  # But this one subdirectory of evil is OK.
  "+base/evil/not",

  # And it can include files from this other directory even though there is
  # no deps rule for it.
  "+tools/crime_fighter",

  # This dependency is allowed for now but work is ongoing to remove it,
  # so you shouldn't add further dependencies on it.
  "!base/evil/ok_for_now.h",
]
```

If you have certain include rules that should only be applied for some files
within this directory and subdirectories, you can write a section named
`specific_include_rules` that is a hash map of regular expressions to the list
of rules that should apply to files matching them. Note that such rules will
always be applied before the rules from `include_rules` have been applied, but
the order in which rules associated with different regular expressions is
applied is arbitrary.

```
specific_include_rules = {
  ".*_(unit|browser|api)test\.cc": [
    "+libraries/testsupport",
  ],
}
```

To add different dependencies for Java instrumentation and unit tests, the
following regular expressions may be useful:

```
specific_include_rules = {
  '.*UnitTest\.java': [
    # Rules for unit tests.
  ],
  '.*(?<!Unit)Test\.java': [
    # Rules for instrumentation tests.
  ],
}
```

You can optionally ignore the rules inherited from parent directories, similar
to "set noparent" in OWNERS files. For example, adding `noparent = True` in
//ash/components/DEPS will cause rules from //ash/DEPS to be ignored, thereby
forcing each //ash/component/foo to explicitly declare foo's dependencies.

```
noparent = True
```

# Directory structure

DEPS files may be placed anywhere in the tree. Each one applies to all
subdirectories, where there may be more DEPS files that provide additions or
subtractions for their own sub-trees.

There is an implicit rule for the current directory (where the DEPS file lives)
and all of its subdirectories. This prevents you from having to explicitly
allow the current directory everywhere. This implicit rule is applied first, so
you can modify or remove it using the normal include rules.

The rules are processed in order. This means you can explicitly allow a higher
directory and then take away permissions from sub-parts, or the reverse.

Note that all directory separators must be `/` slashes (Unix-style) and not
backslashes. All directories should be relative to the source root and use
only lowercase.
