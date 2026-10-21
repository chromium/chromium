---
name: third_party/avoid-transitional-third-party-directories
rubric: design
severity: should_fix
---

**Rule.** When migrating an existing third-party dependency from pre-built CIPD
or 3PP packages to building from source, do not create a separate transitional
directory (such as `third_party/<dep>-source/` or `third_party/<dep>_source/`).
Integrate the upstream source submodule directly into `third_party/<dep>/src/`
and place the `BUILD.gn` and Chromium integration files directly inside
`third_party/<dep>/`.

**Why.** Creating duplicate or interim dependency directories (like
`<dep>-source`) adds redundant configuration, fragments the repository layout,
and forces downstream consumers to migrate twice. Placing the source submodule
in `third_party/<dep>/src/` does not collide with existing `3pp/` package
recipes, and previously published CIPD packages remain accessible in CIPD.

## Examples

Bad:

```cpp
# In .gitmodules:
[submodule "third_party/pipewire-source/src"]
	path = third_party/pipewire-source/src

# In third_party/pipewire-source/BUILD.gn:
config("pipewire_source_internal_config") { ... }
```

Good:

```cpp
# In .gitmodules:
[submodule "third_party/pipewire/src"]
	path = third_party/pipewire/src

# In third_party/pipewire/BUILD.gn:
config("pipewire_internal_config") { ... }
```
