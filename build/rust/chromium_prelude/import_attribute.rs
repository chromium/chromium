// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use proc_macro2::Span;
use quote::quote;
use syn::parse::{Parse, ParseStream};
use syn::punctuated::Punctuated;
use syn::{parse_macro_input, Error, Ident, Lit, Token, Visibility};

#[proc_macro]
pub fn import(input: proc_macro::TokenStream) -> proc_macro::TokenStream {
    // TODO: consider using `Token[,]` here, because `rustfmt` may have an
    // easier time with `chromium::import!(... , ... , ...)`.
    type ImportList = Punctuated<Import, Token![;]>;
    let imports = parse_macro_input!(input with ImportList::parse_terminated);

    let mut stream = proc_macro2::TokenStream::new();
    for i in imports {
        let visibility = &i.visibility;
        let mangled_crate_name = &i.target.mangled_crate_name;
        let name = i.alias.as_ref().unwrap_or(&i.target.gn_name);
        stream.extend(quote! {
          #visibility extern crate #mangled_crate_name as #name;
        });
    }
    stream.into()
}

struct Import {
    target: GnTarget,
    alias: Option<Ident>,
    visibility: Option<Visibility>,
}

struct GnTarget {
    mangled_crate_name: Ident,
    gn_name: Ident,
}

impl GnTarget {
    fn parse(s: &str, span: Span) -> Result<GnTarget, String> {
        if !s.starts_with("//") {
            return Err(String::from("expected absolute GN path (should start with //)"));
        }

        let mut path: Vec<&str> = s[2..].split('/').collect();

        let gn_name = {
            if path.starts_with(&["third_party", "rust"]) {
                return Err(String::from(
                    "import! macro should not be used for third_party crates",
                ));
            }

            let last = path.pop().unwrap();
            let (split_last, gn_name) = match last.split_once(':') {
                Some((last, name)) => (last, name),
                None => (last, last),
            };
            path.push(split_last);

            gn_name
        };

        for p in &path {
            if p.contains(':') {
                return Err(String::from("unexpected ':' in GN path component"));
            }
            if p.is_empty() {
                return Err(String::from("unexpected empty GN path component"));
            }
        }

        let gn_dir = format!("//{}", path.join("/"));
        let mangled_crate_name = mangle_crate_name(&gn_dir, gn_name);

        Ok(GnTarget {
            mangled_crate_name: Ident::new(&mangled_crate_name, span),
            gn_name: syn::parse_str::<Ident>(gn_name).map_err(|e| format!("{e}"))?,
        })
    }
}

impl Parse for Import {
    fn parse(input: ParseStream) -> syn::Result<Self> {
        let visibility = Visibility::parse(input).ok();

        let str_span = input.span();

        let target = match Lit::parse(input) {
            Err(_) => {
                return Err(Error::new(str_span, "expected a GN path as a string literal"));
            }
            Ok(Lit::Str(label)) => match GnTarget::parse(&label.value(), str_span) {
                Ok(target) => target,
                Err(e) => {
                    return Err(Error::new(
                        str_span,
                        format!("invalid GN path {}: {}", quote::quote! {#label}, e),
                    ));
                }
            },
            Ok(lit) => {
                return Err(Error::new(
                    str_span,
                    format!(
                        "expected a GN path as string literal, found '{}' literal",
                        quote::quote! {#lit}
                    ),
                ));
            }
        };
        let alias = match <Token![as]>::parse(input) {
            Ok(_) => Some(Ident::parse(input)?),
            Err(_) => None,
        };

        Ok(Import { target, alias, visibility })
    }
}

/// Mangles a GN target path into a crate name that is globally unique
/// ("globally" means: within a GN build graph).
///
/// NOTE: When updating GnTarget=>CrateName mangling algorithm, it needs to
/// be updated and kept in sync in 3 places: this function,
/// `//build/rust/rust_target.gni`, `//build/rust/rust_static_library.gni`.
///
/// # Example
///
/// ```
/// // Calculate mangled name for the
/// // `//build/rust/chromium_prelude:import_test_lib` GN target:
/// let mangled = mangle_crate_name("//build/rust/chromium_prelude", "import_test_lib");
/// assert_eq!(mangled, "import_test_lib_8086ab2e");
/// ```
fn mangle_crate_name(dir: &str, target_name: &str) -> String {
    assert!(!target_name.is_empty()); // Caller is expected to verify.
    let dir_hash = gn_string_hash(dir);
    format!("{target_name}_{dir_hash}")
}

/// `fn gn_string_hash` replicates `string_hash` from GN [1] which computes
/// the first 8 hexadecimal digits of an SHA256 hash of a string.
///
/// [1] https://gn.googlesource.com/gn/+/main/docs/reference.md#func_string_hash
fn gn_string_hash(s: &str) -> String {
    let sha256 = {
        let mut hasher = hmac_sha256::Hash::new();
        hasher.update(s.as_bytes());
        hasher.finalize()
    };
    let mut result = String::with_capacity(8);
    for byte in sha256.into_iter().take(4) {
        // `unwrap` is okay because writing into a `String` cannot fail.
        use std::fmt::Write;
        write!(&mut result, "{byte:02x}").unwrap();
    }
    result
}
