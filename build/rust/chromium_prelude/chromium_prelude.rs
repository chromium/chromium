// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/// The `chromium::import!{}` macro for importing crates from GN paths.
///
/// This macro is used to access first-party crates in the Chromium project
/// (or other projects using Chromium's //build system) through the GN path
/// to the crate. All GN paths must be absolute paths.
///
/// Third-party crates are accessed as usual by their name, which is available
/// whenever the Rust target depends on the third-party crate. The `import!`
/// macro does nothing, and will cause a compilation error if an attempt is
/// made to import a third-party crate with it.
///
/// # Motivation
///
/// Since first-party crates do not all have globally unique GN names, using
/// their GN target name as their crate name leads to ambiguity when multiple
/// crates with the same name are dependencies of the same crate. As such, we
/// produce mangled crate names that are unique, and depend on their full GN
/// path, but these names are not easy to spell.
///
/// # Usage
///
/// The `chromium` crate is automatically present in all first-party Rust
/// targets, which makes the `chromium::import!{}` macro available. The macro
/// should be given a list of GN paths (directory and target name) as quoted
/// strings to be imported into the current module, delineated with semicolons.
///
/// When no target name is specified (e.g. `:name`) at the end of the GN path,
/// the target will be the same as the last directory name. This is the same
/// behaviour as within the GN `deps` list.
///
/// The imported crates can be renamed by using the `as` keyword and reexported
/// using the `pub` keyword. These function in the same way as they do when
/// naming or reexporting with `use`, but the `import!` macro replaces the `use`
/// keyword for these purposes with first-party crates.
///
/// # Examples
///
/// ## Basic usage
/// Basic usage, importing a few targets. The name given to the imported crates
/// is their GN target name by default. In this example, there would be two
/// crates available in the Rust module below: `example` which is the
/// `example` GN target in `rust/example/BUILD.gn` and `other` which is the
/// `other` GN target in `rust/example/BUILD.gn`.
/// ```
/// chromium::import! {
///   "//rust/example";
///   "//rust/example:other";
/// }
///
/// use example::Goat;
///
/// example::foo(Goat::new());
/// other::foo(Goat::with_age(3));
/// ```
///
/// ## Renaming an import
/// Since multiple GN targets may have the same local name, they can be given
/// a different name when imported by using `as`:
/// ```
/// chromium::import! {
///   "//rust/example" as renamed;
///   "//rust/example:other" as very_renamed;
/// }
///
/// use renamed::Goat;
///
/// renamed::foo(Goat::new());
/// very_renamed::foo(Goat::with_age(3));
/// ```
///
/// ## Re-exporting
/// When importing and re-exporting a dependency, the usual syntax would be
/// `pub use my_dependency;`. For first-party crates, this must be done through
/// the `import!` macro by prepending the `pub` keyword where the crate is
/// imported. The exported name can be specified with `as`:
/// ```
/// mod module {
///
/// chromium::import! {
///   pub "//rust/example";
///   pub "//rust/example:other" as exported_other;
/// }
///
/// }
///
/// use module::example::Goat;
///
/// module::example::foo(Goat::new())
/// module::exported_other::foo(Goat::with_age(3));
/// ```
pub use import_attribute::import;
