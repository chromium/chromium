// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use proc_macro::TokenStream;
use quote::quote;
use syn::{parse_macro_input, DeriveInput};

#[proc_macro_derive(UmaEnum)]
pub fn derive_uma_enum(input: TokenStream) -> TokenStream {
    let input = parse_macro_input!(input as DeriveInput);
    let name = &input.ident;

    let syn::Data::Enum(syn::DataEnum { variants, .. }) = &input.data else {
        return syn::Error::new_spanned(name, "UmaEnum can only be derived for enums.")
            .to_compile_error()
            .into();
    };

    if variants.is_empty() {
        return syn::Error::new_spanned(name, "UmaEnum cannot be derived for empty enums.")
            .to_compile_error()
            .into();
    }

    if let Some(variant) = variants.iter().find(|v| !v.fields.is_empty()) {
        return syn::Error::new_spanned(variant, "UmaEnum variants must not have fields.")
            .to_compile_error()
            .into();
    }

    let variant_idents = variants.iter().map(|v| &v.ident);
    let (impl_generics, ty_generics, where_clause) = input.generics.split_for_impl();

    let expanded = quote! {
        const _: () = {
            chromium::import! {
                "//base:histogram";
            }

            impl #impl_generics histogram::UmaEnum for #name #ty_generics #where_clause {
                const MAX_VALUE: i32 = {
                    let mut max = 0;
                    #(
                        // We must use a cast, as that's the only way to get at the underlying discriminant.
                        // However, `as` is a truncating cast. So we cast to i128, since that's the largest
                        // primitive type (large u128 will get wrapped to negatives, which is fine).
                        let val = Self::#variant_idents as i128;
                        assert!(
                            val >= 0 && val <= 1000,
                            "UmaEnum variant discriminant must be between 0 and 1000 inclusive."
                        );
                        if val > max {
                            max = val;
                        }
                    )*
                    max as i32
                };

                fn to_sample(self) -> i32 {
                    self as i32
                }
            }
        };
    };

    expanded.into()
}
