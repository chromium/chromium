// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use proc_macro2::Span;
use quote::quote;
use syn::parse::{Parse, ParseStream};
use syn::{parse_macro_input, Error, Ident, Lit, Token};

#[proc_macro]
pub fn import(input: proc_macro::TokenStream) -> proc_macro::TokenStream {
    let imports = parse_macro_input!(input as ImportList).imports;

    let mut stream = proc_macro2::TokenStream::new();
    for i in imports {
        let public = &i.reexport;
        let mangled_crate_name = &i.target.mangled_crate_name;
        let name = i.alias.as_ref().unwrap_or(&i.target.gn_name);
        stream.extend(quote! {
          #public extern crate #mangled_crate_name as #name;
        });
    }
    stream.into()
}

struct ImportList {
    imports: Vec<Import>,
}

struct Import {
    target: GnTarget,
    alias: Option<Ident>,
    reexport: Option<Token![pub]>,
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

        let mangled_crate_name =
            escape_non_identifier_chars(&format!("{}:{gn_name}", path.join("/")))?;

        Ok(GnTarget {
            mangled_crate_name: Ident::new(&mangled_crate_name, span),
            gn_name: syn::parse_str::<Ident>(gn_name).map_err(|e| format!("{e}"))?,
        })
    }
}

impl Parse for ImportList {
    fn parse(input: ParseStream) -> syn::Result<Self> {
        let mut imports: Vec<Import> = Vec::new();

        while !input.is_empty() {
            let reexport = <Token![pub]>::parse(input).ok();

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
            <syn::Token![;]>::parse(input)?;

            imports.push(Import { target, alias, reexport });
        }

        Ok(Self { imports })
    }
}

/// Escapes non-identifier characters in `symbol`.
///
/// Importantly, this is
/// [an injective function](https://en.wikipedia.org/wiki/Injective_function)
/// which means that different inputs are never mapped to the same output.
///
/// This is based on a similar function in
/// https://github.com/google/crubit/blob/22ab04aef9f7cc56d8600c310c7fe20999ffc41b/common/code_gen_utils.rs#L59-L71
/// The main differences are:
///
/// * Only a limited set of special characters is supported, because this makes
///   it easier to replicate the escaping algorithm in `.gni` files, using just
///   `string_replace` calls.
/// * No dependency on `unicode_ident` crate means that instead of
///   `is_xid_continue` a more restricted call to `char::is_ascii_alphanumeric`
///   is used.
/// * No support for escaping leading digits.
/// * The escapes are slightly different (e.g. `/` frequently appears in GN
///   paths and therefore here we map it to a nice `_s` rather than to `_x002f`)
fn escape_non_identifier_chars(symbol: &str) -> Result<String, String> {
    assert!(!symbol.is_empty()); // Caller is expected to verify.
    if symbol.chars().next().unwrap().is_ascii_digit() {
        return Err("Leading digits are not supported".to_string());
    }

    // Escaping every character can at most double the size of the string.
    let mut result = String::with_capacity(symbol.len() * 2);
    for c in symbol.chars() {
        // NOTE: TargetName=>CrateName mangling algorithm should be updated
        // simultaneously in 3 places: here, //build/rust/rust_target.gni,
        // //build/rust/rust_static_library.gni.
        match c {
            '_' => result.push_str("_u"),
            '/' => result.push_str("_s"),
            ':' => result.push_str("_c"),
            '-' => result.push_str("_d"),
            c if c.is_ascii_alphanumeric() => result.push(c),
            _ => return Err(format!("Unsupported character in GN path component: `{c}`")),
        }
    }

    Ok(result)
}
